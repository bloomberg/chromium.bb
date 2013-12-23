// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_FOREIGN_SESSION_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_FOREIGN_SESSION_HANDLER_H_

#include <vector>

#include "base/time/time.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sync/open_tabs_ui_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace browser_sync {

class ForeignSessionHandler : public content::WebUIMessageHandler,
                              public content::NotificationObserver {
 public:
  // Invalid value, used to note that we don't have a tab or window number.
  static const int kInvalidId = -1;

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  ForeignSessionHandler();
  virtual ~ForeignSessionHandler() {}

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  static void OpenForeignSessionTab(content::WebUI* web_ui,
                                    const std::string& session_string_value,
                                    SessionID::id_type window_num,
                                    SessionID::id_type tab_id,
                                    const WindowOpenDisposition& disposition);

  static void OpenForeignSessionWindows(content::WebUI* web_ui,
                                        const std::string& session_string_value,
                                        SessionID::id_type window_num);

  // Helper method to create JSON compatible objects from Session objects.
  static bool SessionTabToValue(const SessionTab& tab,
                                base::DictionaryValue* dictionary);

  // Returns a pointer to the current session model associator or NULL.
  static OpenTabsUIDelegate* GetOpenTabsUIDelegate(content::WebUI* web_ui);

 private:
  // Used to register ForeignSessionHandler for notifications.
  void Init();

  // Determines how ForeignSessionHandler will interact with the new tab page.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Returns true if tab sync is enabled for this profile, otherwise false.
  bool IsTabSyncEnabled();

  // Returns a string used to show the user when a session was last modified.
  base::string16 FormatSessionTime(const base::Time& time);

  // Determines which session is to be opened, and then calls
  // OpenForeignSession, to begin the process of opening a new browser window.
  // This is a javascript callback handler.
  void HandleOpenForeignSession(const base::ListValue* args);

  // Determines whether foreign sessions should be obtained from the sync model.
  // This is a javascript callback handler, and it is also called when the sync
  // model has changed and the new tab page needs to reflect the changes.
  void HandleGetForeignSessions(const base::ListValue* args);

  // Delete a foreign session. This will remove it from the list of foreign
  // sessions on all devices. It will reappear if the session is re-activated
  // on the original device.
  // This is a javascript callback handler.
  void HandleDeleteForeignSession(const base::ListValue* args);

  void HandleSetForeignSessionCollapsed(const base::ListValue* args);

  // Helper method to create JSON compatible objects from Session objects.
  bool SessionWindowToValue(const SessionWindow& window,
                            base::DictionaryValue* dictionary);

  // The Registrar used to register ForeignSessionHandler for notifications.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ForeignSessionHandler);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_FOREIGN_SESSION_HANDLER_H_
