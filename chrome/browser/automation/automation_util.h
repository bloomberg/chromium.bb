// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOMATION_AUTOMATION_UTIL_H_
#define CHROME_BROWSER_AUTOMATION_AUTOMATION_UTIL_H_
#pragma once

#include <string>

#include "base/basictypes.h"

class AutomationProvider;
class Browser;
class DictionaryValue;
class GURL;
class TabContents;

namespace IPC {
class Message;
}

// This file contains automation utility functions.

namespace automation_util {

// Returns the browser at the given index of the |BrowserList| or NULL if the
// index is out of range.
Browser* GetBrowserAt(int index);

// Returns the tab at |tab_index| within the browser at |browser_index| in the
// |BrowserList|. If any of these indices are invalid, NULL will be returned.
TabContents* GetTabContentsAt(int browser_index, int tab_index);

// Gets the size and value of the cookie string for |url| in the given tab.
// Can be called from any thread.
void GetCookies(const GURL& url,
                TabContents* contents,
                int* value_size,
                std::string* value);

// Sets a cookie for |url| in the given tab.  Can be called from any thread.
void SetCookie(const GURL& url,
               const std::string& value,
               TabContents* contents,
               int* response_value);

// Deletes a cookie for |url| in the given tab.  Can be called from any thread.
void DeleteCookie(const GURL& url,
                  const std::string& cookie_name,
                  TabContents* contents,
                  bool* success);

// Gets the cookies for the given URL. Uses the JSON interface.
// See |TestingAutomationProvider| for example input.
void GetCookiesJSON(AutomationProvider* provider,
                    DictionaryValue* args,
                    IPC::Message* reply_message);

// Deletes the cookie with the given name for the URL. Uses the JSON interface.
// See |TestingAutomationProvider| for example input.
void DeleteCookieJSON(AutomationProvider* provider,
                      DictionaryValue* args,
                      IPC::Message* reply_message);

// Sets a cookie for the given URL. Uses the JSON interface.
// See |TestingAutomationProvider| for example input.
void SetCookieJSON(AutomationProvider* provider,
                   DictionaryValue* args,
                   IPC::Message* reply_message);

}  // namespace automation_util

#endif  // CHROME_BROWSER_AUTOMATION_AUTOMATION_UTIL_H_
