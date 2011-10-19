// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_NTP_LOGIN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_NTP_LOGIN_HANDLER_H_
#pragma once

#include "chrome/browser/prefs/pref_member.h"
#include "content/browser/webui/web_ui.h"
#include "content/public/browser/notification_observer.h"

class Profile;
class Browser;

// The NTP login handler currently simply displays the current logged in
// username at the top of the NTP (and update itself when that changes).
// In the future it may expand to allow users to login from the NTP.
class NTPLoginHandler : public WebUIMessageHandler,
                        public content::NotificationObserver {
 public:
  NTPLoginHandler();
  virtual ~NTPLoginHandler();

  virtual WebUIMessageHandler* Attach(WebUI* web_ui) OVERRIDE;

  // WebUIMessageHandler interface
  virtual void RegisterMessages() OVERRIDE;

  // content::NotificationObserver interface
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Returns true if the login handler should be shown in a new tab page
  // for the given |profile|. |profile| must not be NULL.
  static bool ShouldShow(Profile* profile);

 private:
  // Called from JS when the NTP is loaded. |args| is the list of arguments
  // passed from JS and should be an empty list.
  void HandleInitializeSyncLogin(const ListValue* args);

  // Called from JS when the user clicks the login container. It shows the
  // appropriate UI based on the current sync state. |args| is the list of
  // arguments passed from JS and should be an empty list.
  void HandleShowSyncLoginUI(const ListValue* args);

  // Internal helper method
  void UpdateLogin();

  StringPrefMember username_pref_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_NTP_LOGIN_HANDLER_H_
