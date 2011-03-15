// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOMATION_AUTOMATION_UTIL_H_
#define CHROME_BROWSER_AUTOMATION_AUTOMATION_UTIL_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/automation/automation_provider.h"
#include "content/browser/tab_contents/tab_contents.h"

class DictionaryValue;

// This file contains automation utility functions.

namespace automation_util {

// Gets the size and value of the cookie string for |url| in the given tab.
// Can be called from any thread.
void GetCookies(const GURL& url,
                TabContents* contents,
                int* value_size,
                std::string* value);

// Sets a cookie for |url| in the given tab.  Can be called from any thread.
void SetCookie(const GURL& url,
               const std::string value,
               TabContents* contents,
               int* response_value);

// Deletes a cookie for |url| in the given tab.  Can be called from any thread.
void DeleteCookie(const GURL& url,
                  const std::string& cookie_name,
                  TabContents* contents,
                  bool* success);

// Gets the cookies for the given URL. Uses the JSON interface.
// Example:
//   input: { "windex": 1, "tab_index": 1, "url": "http://www.google.com" }
//   output: { "cookies": "PREF=12012" }
void GetCookiesJSON(AutomationProvider* provider,
                    DictionaryValue* args,
                    IPC::Message* reply_message);

// Deletes the cookie with the given name for the URL. Uses the JSON interface.
// Example:
//   input: { "windex": 1,
//            "tab_index": 1,
//            "url": "http://www.google.com",
//            "name": "my_cookie"
//          }
//   output: none
void DeleteCookieJSON(AutomationProvider* provider,
                      DictionaryValue* args,
                      IPC::Message* reply_message);

// Sets a cookie for the given URL. Uses the JSON interface.
// Example:
//   input: { "windex": 1,
//            "tab_index": 1,
//            "url": "http://www.google.com",
//            "cookie": "PREF=21321"
//          }
//   output: none
void SetCookieJSON(AutomationProvider* provider,
                   DictionaryValue* args,
                   IPC::Message* reply_message);

}  // namespace automation_util

#endif  // CHROME_BROWSER_AUTOMATION_AUTOMATION_UTIL_H_
