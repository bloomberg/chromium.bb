// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Lists the APIs to be registered, and in which modules to register them.
// This is to give us a single place to specify the list of APIs to handle
// (in the broker) and to redirect (in each Chrome Frame host - which is not
// necessarily in the broker and thus wouldn't have a ProductionApiDispatcher
// object available to query for registered functions).

#ifndef CEEE_IE_COMMON_API_REGISTRATION_H_
#define CEEE_IE_COMMON_API_REGISTRATION_H_

#include "chrome/browser/extensions/execute_code_in_tab_function.h"
#include "chrome/browser/extensions/extension_cookies_api.h"
#include "chrome/browser/extensions/extension_infobar_module.h"
#include "chrome/browser/extensions/extension_tabs_module.h"

#ifdef REGISTER_API_FUNCTION
#error Must change the name of the REGISTER_API_FUNCTION macro in this file
#endif  // REGISTER_API_FUNCTION

// You must define a REGISTER_API_FUNCTION macro in the compilation unit where
// you include this file, before invoking any of the REGISTER_XYZ_API_FUNCTIONS
// macros.
//
// In the compilation unit that registers function groups with the production
// API dispatcher, define PRODUCTION_API_DISPATCHER and your own definition of
// each of these macros.  This may seem a bit roundabout, it's just done this
// way to ensure that if you add function groups to this file and forget to
// update the production API dispatcher you'll get a compilation error.

#ifndef PRODUCTION_API_DISPATCHER

// Registers the chrome.tab.* functions we handle.
#define REGISTER_TAB_API_FUNCTIONS() \
  REGISTER_API_FUNCTION(GetTab); \
  REGISTER_API_FUNCTION(GetCurrentTab); \
  REGISTER_API_FUNCTION(GetSelectedTab); \
  REGISTER_API_FUNCTION(GetAllTabsInWindow); \
  REGISTER_API_FUNCTION(CreateTab); \
  REGISTER_API_FUNCTION(UpdateTab); \
  REGISTER_API_FUNCTION(MoveTab); \
  REGISTER_API_FUNCTION(TabsExecuteScript); \
  REGISTER_API_FUNCTION(TabsInsertCSS); \
  REGISTER_API_FUNCTION(RemoveTab)

// Registers the chrome.window.* functions we handle.
// We don't have CreateWindow in here because of a compilation limitation.
// See the comment in the windows_api::CreateWindowFunc class for more details.
#define REGISTER_WINDOW_API_FUNCTIONS() \
  REGISTER_API_FUNCTION(GetWindow); \
  REGISTER_API_FUNCTION(GetCurrentWindow); \
  REGISTER_API_FUNCTION(GetLastFocusedWindow); \
  REGISTER_API_FUNCTION(UpdateWindow); \
  REGISTER_API_FUNCTION(RemoveWindow); \
  REGISTER_API_FUNCTION(GetAllWindows)

// Registers the chrome.cookies.* functions we handle.
#define REGISTER_COOKIE_API_FUNCTIONS() \
  REGISTER_API_FUNCTION(GetCookie); \
  REGISTER_API_FUNCTION(GetAllCookies); \
  REGISTER_API_FUNCTION(SetCookie); \
  REGISTER_API_FUNCTION(RemoveCookie); \
  REGISTER_API_FUNCTION(GetAllCookieStores)

// Registers the chrome.experimental.infobars.* functions we handle.
#define REGISTER_INFOBAR_API_FUNCTIONS() \
  REGISTER_API_FUNCTION(ShowInfoBar)

// Although we don't need to handle any chrome.experimental.webNavigation.*
// functions, we use this macro to register permanent event handlers when
// PRODUCTION_API_DISPATCHER is defined.
#define REGISTER_WEBNAVIGATION_API_FUNCTIONS()

// Although we don't need to handle any chrome.experimental.webRequest.*
// functions, we use this macro to register permanent event handlers when
// PRODUCTION_API_DISPATCHER is defined.
#define REGISTER_WEBREQUEST_API_FUNCTIONS()

// Add new tab function groups before this line.
#endif  // PRODUCTION_API_DISPATCHER

// Call this to register all functions.  If you don't define
// PRODUCTION_API_DISPATCHER it will simply cause REGISTER_FUNCTION to be called
// for all functions.  Otherwise it will cause your custom implementation of
// REGISTER_XYZ_API_FUNCTIONS to be called for each of the function groups
// above.
#define REGISTER_ALL_API_FUNCTIONS() \
  REGISTER_TAB_API_FUNCTIONS(); \
  REGISTER_WINDOW_API_FUNCTIONS(); \
  REGISTER_COOKIE_API_FUNCTIONS(); \
  REGISTER_INFOBAR_API_FUNCTIONS(); \
  REGISTER_WEBNAVIGATION_API_FUNCTIONS(); \
  REGISTER_WEBREQUEST_API_FUNCTIONS()

#endif  // CEEE_IE_COMMON_API_REGISTRATION_H_
