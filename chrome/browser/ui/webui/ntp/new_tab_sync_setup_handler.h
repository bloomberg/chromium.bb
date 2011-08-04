// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_NEW_TAB_SYNC_SETUP_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_NEW_TAB_SYNC_SETUP_HANDLER_H_

#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/ui/webui/sync_setup_handler.h"

// The handler for Javascript messages related to the sync setup UI in the new
// tab page.
class NewTabSyncSetupHandler : public SyncSetupHandler {
 public:
  NewTabSyncSetupHandler();
  virtual ~NewTabSyncSetupHandler();

  // Checks if the sync promo should be visible.
  static bool ShouldShowSyncPromo();

  // WebUIMessageHandler implementation.
  virtual WebUIMessageHandler* Attach(WebUI* web_ui);
  virtual void RegisterMessages();

  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 protected:
  virtual void ShowSetupUI();

  // Javascript callback handler to initialize the sync promo UI.
  void HandleInitializeSyncPromo(const ListValue* args);

  // Javascript callback handler to collapse the sync promo.
  void HandleCollapseSyncPromo(const ListValue* args);

  // Javascript callback handler to expand the sync promo.
  void HandleExpandSyncPromo(const ListValue* args);

 private:
  // Sends the sync login name to the page.
  void UpdateLogin();

  // Saves the expanded state to preferences.
  void SaveExpandedPreference(bool is_expanded);

  // Preference for the sync login name.
  StringPrefMember username_pref_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_NEW_TAB_SYNC_SETUP_HANDLER_H_
