// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBUI_NTP_LOGIN_HANDLER_H_
#define CHROME_BROWSER_WEBUI_NTP_LOGIN_HANDLER_H_
#pragma once

#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/webui/web_ui.h"
#include "chrome/common/notification_observer.h"

// The NTP login handler currently simply displays the current logged in
// username at the top of the NTP (and update itself when that changes).
// In the future it may expand to allow users to login from the NTP.
class NTPLoginHandler : public WebUIMessageHandler,
                        public NotificationObserver {
 public:
  NTPLoginHandler();
  ~NTPLoginHandler();

  virtual WebUIMessageHandler* Attach(WebUI* web_ui);

  // WebUIMessageHandler interface
  virtual void RegisterMessages();

  // NotificationObserver interface
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  // Called from JS when the NTP is loaded.
  void HandleInitializeLogin(const ListValue* args);

  // Internal helper method
  void UpdateLogin();

  StringPrefMember username_pref_;
};

#endif  // CHROME_BROWSER_WEBUI_NTP_LOGIN_HANDLER_H_
