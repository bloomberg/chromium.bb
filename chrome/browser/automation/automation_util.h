// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOMATION_AUTOMATION_UTIL_H_
#define CHROME_BROWSER_AUTOMATION_AUTOMATION_UTIL_H_
#pragma once

#include <string>

#include "base/basictypes.h"

class AutomationId;
class AutomationProvider;
class Browser;
class Extension;
class ExtensionHost;
class GURL;
class Profile;
class RenderViewHost;
class TabContents;
class TabContentsWrapper;

namespace base {
class DictionaryValue;
}

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

// Returns the browser that contains the given tab, or NULL if none exists.
Browser* GetBrowserForTab(TabContents* tab);

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
                    base::DictionaryValue* args,
                    IPC::Message* reply_message);

// Deletes the cookie with the given name for the URL. Uses the JSON interface.
// See |TestingAutomationProvider| for example input.
void DeleteCookieJSON(AutomationProvider* provider,
                      base::DictionaryValue* args,
                      IPC::Message* reply_message);

// Sets a cookie for the given URL. Uses the JSON interface.
// See |TestingAutomationProvider| for example input.
void SetCookieJSON(AutomationProvider* provider,
                   base::DictionaryValue* args,
                   IPC::Message* reply_message);

// Sends a JSON error reply if an app modal dialog is active. Returns whether
// an error reply was sent.
bool SendErrorIfModalDialogActive(AutomationProvider* provider,
                                  IPC::Message* message);

// Returns a valid automation ID for the given tab.
AutomationId GetIdForTab(const TabContentsWrapper* tab);

// Returns a valid automation ID for the extension host.
AutomationId GetIdForExtensionView(const ExtensionHost* ext_host);

// Returns a valid automation ID for the extension.
AutomationId GetIdForExtension(const Extension* extension);

// Gets the tab for the given ID. Returns true on success.
bool GetTabForId(const AutomationId& id, TabContents** tab);

// Gets the render view for the given ID. Returns true on success.
bool GetRenderViewForId(const AutomationId& id,
                        Profile* profile,
                        RenderViewHost** rvh);

// Gets the extension for the given ID. Returns true on success.
bool GetExtensionForId(const AutomationId& id,
                       Profile* profile,
                       const Extension** extension);

// Returns whether the given ID refers to an actual automation entity.
bool DoesObjectWithIdExist(const AutomationId& id, Profile* profile);

}  // namespace automation_util

#endif  // CHROME_BROWSER_AUTOMATION_AUTOMATION_UTIL_H_
