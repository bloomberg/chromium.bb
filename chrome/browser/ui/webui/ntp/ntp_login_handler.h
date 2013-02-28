// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_NTP_LOGIN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_NTP_LOGIN_HANDLER_H_

#include "base/prefs/public/pref_member.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_ui_message_handler.h"

class Profile;

// The NTP login handler currently simply displays the current logged in
// username at the top of the NTP (and update itself when that changes).
// In the future it may expand to allow users to login from the NTP.
class NTPLoginHandler : public content::WebUIMessageHandler,
                        public content::NotificationObserver {
 public:
  NTPLoginHandler();
  virtual ~NTPLoginHandler();

  // WebUIMessageHandler interface
  virtual void RegisterMessages() OVERRIDE;

  // content::NotificationObserver interface
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Returns true if the login handler should be shown in a new tab page
  // for the given |profile|. |profile| must not be NULL.
  static bool ShouldShow(Profile* profile);

  // Registers values (strings etc.) for the page.
  static void GetLocalizedValues(Profile* profile, DictionaryValue* values);

 private:
  // User actions while on the NTP when clicking on or viewing the sync promo.
  enum NTPSignInPromoBuckets {
    NTP_SIGN_IN_PROMO_VIEWED,
    NTP_SIGN_IN_PROMO_CLICKED,
    NTP_SIGN_IN_PROMO_BUCKET_BOUNDARY,
  };

  // Called from JS when the NTP is loaded. |args| is the list of arguments
  // passed from JS and should be an empty list.
  void HandleInitializeSyncLogin(const ListValue* args);

  // Called from JS when the user clicks the login container. It shows the
  // appropriate UI based on the current sync state. |args| is the list of
  // arguments passed from JS and should be an empty list.
  void HandleShowSyncLoginUI(const ListValue* args);

  // Records actions in SyncPromo.NTPPromo histogram.
  void RecordInHistogram(int type);

  // Called from JS when the sync promo NTP bubble has been displayed. |args| is
  // the list of arguments passed from JS and should be an empty list.
  void HandleLoginMessageSeen(const ListValue* args);

  // Called from JS when the user clicks on the advanced link the sync promo NTP
  // bubble. Use use this to navigate to the sync settings page. |args| is the
  // list of arguments passed from JS and should be an empty list.
  void HandleShowAdvancedLoginUI(const ListValue* args);

  // Internal helper method
  void UpdateLogin();

  StringPrefMember username_pref_;
  BooleanPrefMember signin_allowed_pref_;
  content::NotificationRegistrar registrar_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_NTP_LOGIN_HANDLER_H_
