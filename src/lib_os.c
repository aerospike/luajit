/*
** OS library.
** Copyright (C) 2005-2022 Mike Pall. See Copyright Notice in luajit.h
**
** Major portions taken verbatim or adapted from the Lua interpreter.
** Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
*/

#include <errno.h>
#include <time.h>

#define lib_os_c
#define LUA_LIB

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_err.h"
#include "lj_buf.h"
#include "lj_str.h"
#include "lj_lib.h"

#if LJ_TARGET_POSIX
#include <unistd.h>
#else
#include <stdio.h>
#endif

#if !LJ_TARGET_PSVITA
#include <locale.h>
#endif

/* ------------------------------------------------------------------------ */

#define LJLIB_MODULE_os

LJLIB_CF(os_clock)
{
  setnumV(L->top++, ((lua_Number)clock())*(1.0/(lua_Number)CLOCKS_PER_SEC));
  return 1;
}

/* ------------------------------------------------------------------------ */

static void setfield(lua_State *L, const char *key, int value)
{
  lua_pushinteger(L, value);
  lua_setfield(L, -2, key);
}

static void setboolfield(lua_State *L, const char *key, int value)
{
  if (value < 0)  /* undefined? */
    return;  /* does not set field */
  lua_pushboolean(L, value);
  lua_setfield(L, -2, key);
}

static int getboolfield(lua_State *L, const char *key)
{
  int res;
  lua_getfield(L, -1, key);
  res = lua_isnil(L, -1) ? -1 : lua_toboolean(L, -1);
  lua_pop(L, 1);
  return res;
}

static int getfield(lua_State *L, const char *key, int d)
{
  int res;
  lua_getfield(L, -1, key);
  if (lua_isnumber(L, -1)) {
    res = (int)lua_tointeger(L, -1);
  } else {
    if (d < 0)
      lj_err_callerv(L, LJ_ERR_OSDATEF, key);
    res = d;
  }
  lua_pop(L, 1);
  return res;
}

LJLIB_CF(os_date)
{
  const char *s = luaL_optstring(L, 1, "%c");
  time_t t = luaL_opt(L, (time_t)luaL_checknumber, 2, time(NULL));
  struct tm *stm;
#if LJ_TARGET_POSIX
  struct tm rtm;
#endif
  if (*s == '!') {  /* UTC? */
    s++;  /* Skip '!' */
#if LJ_TARGET_POSIX
    stm = gmtime_r(&t, &rtm);
#else
    stm = gmtime(&t);
#endif
  } else {
#if LJ_TARGET_POSIX
    stm = localtime_r(&t, &rtm);
#else
    stm = localtime(&t);
#endif
  }
  if (stm == NULL) {  /* Invalid date? */
    setnilV(L->top++);
  } else if (strcmp(s, "*t") == 0) {
    lua_createtable(L, 0, 9);  /* 9 = number of fields */
    setfield(L, "sec", stm->tm_sec);
    setfield(L, "min", stm->tm_min);
    setfield(L, "hour", stm->tm_hour);
    setfield(L, "day", stm->tm_mday);
    setfield(L, "month", stm->tm_mon+1);
    setfield(L, "year", stm->tm_year+1900);
    setfield(L, "wday", stm->tm_wday+1);
    setfield(L, "yday", stm->tm_yday+1);
    setboolfield(L, "isdst", stm->tm_isdst);
  } else if (*s) {
    SBuf *sb = &G(L)->tmpbuf;
    MSize sz = 0, retry = 4;
    const char *q;
    for (q = s; *q; q++)
      sz += (*q == '%') ? 30 : 1;  /* Overflow doesn't matter. */
    setsbufL(sb, L);
    while (retry--) {  /* Limit growth for invalid format or empty result. */
      char *buf = lj_buf_need(sb, sz);
      size_t len = strftime(buf, sbufsz(sb), s, stm);
      if (len) {
	setstrV(L, L->top++, lj_str_new(L, buf, len));
	lj_gc_check(L);
	break;
      }
      sz += (sz|1);
    }
  } else {
    setstrV(L, L->top++, &G(L)->strempty);
  }
  return 1;
}

LJLIB_CF(os_time)
{
  time_t t;
  if (lua_isnoneornil(L, 1)) {  /* called without args? */
    t = time(NULL);  /* get current time */
  } else {
    struct tm ts;
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_settop(L, 1);  /* make sure table is at the top */
    ts.tm_sec = getfield(L, "sec", 0);
    ts.tm_min = getfield(L, "min", 0);
    ts.tm_hour = getfield(L, "hour", 12);
    ts.tm_mday = getfield(L, "day", -1);
    ts.tm_mon = getfield(L, "month", -1) - 1;
    ts.tm_year = getfield(L, "year", -1) - 1900;
    ts.tm_isdst = getboolfield(L, "isdst");
    t = mktime(&ts);
  }
  if (t == (time_t)(-1))
    lua_pushnil(L);
  else
    lua_pushnumber(L, (lua_Number)t);
  return 1;
}

LJLIB_CF(os_difftime)
{
  lua_pushnumber(L, difftime((time_t)(luaL_checknumber(L, 1)),
			     (time_t)(luaL_optnumber(L, 2, (lua_Number)0))));
  return 1;
}

/* ------------------------------------------------------------------------ */

#include "lj_libdef.h"

LUALIB_API int luaopen_os(lua_State *L)
{
  LJ_LIB_REG(L, LUA_OSLIBNAME, os);
  return 1;
}

