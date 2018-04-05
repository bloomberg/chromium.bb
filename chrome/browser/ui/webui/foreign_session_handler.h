// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FOREIGN_SESSION_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_FOREIGN_SESSION_HANDLER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "chrome/browser/sessions/session_service.h"
#include "components/sync/driver/sync_service_observer.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace syncer {
class SyncService;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace browser_sync {

class ForeignSessionHandler : public content::WebUIMessageHandler,
                              public syncer::SyncServiceObserver {
 public:
  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  ForeignSessionHandler();
  ~ForeignSessionHandler() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  static void OpenForeignSessionTab(content::WebUI* web_ui,
                                    const std::string& session_string_value,
                                    int window_num,
                                    SessionID tab_id,
                                    const WindowOpenDisposition& disposition);

  static void OpenForeignSessionWindows(content::WebUI* web_ui,
                                        const std::string& session_string_value,
                                        int window_num);

  // Returns a pointer to the current session model associator or NULL.
  static sync_sessions::OpenTabsUIDelegate* GetOpenTabsUIDelegate(
      content::WebUI* web_ui);

 private:
  // syncer::SyncServiceObserver:
  void OnSyncConfigurationCompleted(syncer::SyncService* sync) override;
  void OnForeignSessionUpdated(syncer::SyncService* sync) override;

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

  // ScopedObserver used to observe the ProfileSyncService.
  ScopedObserver<syncer::SyncService, syncer::SyncServiceObserver>
      scoped_observer_;

  // The time at which this WebUI was created. Used to calculate how long
  // the WebUI was present before the sessions data was visible.
  base::TimeTicks load_attempt_time_;

  DISALLOW_COPY_AND_ASSIGN(ForeignSessionHandler);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_UI_WEBUI_FOREIGN_SESSION_HANDLER_H_
