// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_CLEAR_BROWSING_DATA_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_CLEAR_BROWSING_DATA_HANDLER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/browsing_data/browsing_data_counter.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/prefs/pref_change_registrar.h"

namespace base {
class ListValue;
}

namespace content {
class WebUI;
}

namespace settings {

// Chrome browser startup settings handler.
class ClearBrowsingDataHandler : public SettingsPageUIHandler,
                                 public BrowsingDataRemover::Observer,
                                 public sync_driver::SyncServiceObserver {
 public:
  explicit ClearBrowsingDataHandler(content::WebUI* webui);
  ~ClearBrowsingDataHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  // Clears browsing data, called by Javascript.
  void HandleClearBrowsingData(const base::ListValue* value);

  // BrowsingDataRemover::Observer implementation.
  // Re-enables clear button once all requested data has been removed.
  void OnBrowsingDataRemoverDone() override;

  // Updates UI when the pref to allow clearing history changes.
  virtual void OnBrowsingHistoryPrefChanged();

  // Initializes the dialog UI. Called by JavaScript when the DOM is ready.
  void HandleInitialize(const base::ListValue* args);

  // Implementation of SyncServiceObserver. Updates the footer of the dialog
  // when the sync state changes.
  void OnStateChanged() override;

  // Finds out whether we should show notice about other forms of history stored
  // in user's account.
  void RefreshHistoryNotice();

  // Called as an asynchronous response to |RefreshHistoryNotice()|. Shows or
  // hides the footer about other forms of history stored in user's account.
  void UpdateHistoryNotice(bool show);

  // Called as an asynchronous response to |RefreshHistoryNotice()|. Enables or
  // disables the dialog about other forms of history stored in user's account
  // that is shown when the history deletion is finished.
  void UpdateHistoryDeletionDialog(bool show);

  // Adds a browsing data |counter|.
  void AddCounter(std::unique_ptr<BrowsingDataCounter> counter);

  // Updates a counter text according to the |result|.
  void UpdateCounterText(std::unique_ptr<BrowsingDataCounter::Result> result);

  // Cached profile corresponding to the WebUI of this handler.
  Profile* profile_;

  // Counters that calculate the data volume for individual data types.
  std::vector<std::unique_ptr<BrowsingDataCounter>> counters_;

  // ProfileSyncService to observe sync state changes.
  ProfileSyncService* sync_service_;
  ScopedObserver<ProfileSyncService, sync_driver::SyncServiceObserver>
      sync_service_observer_;

  // If non-null it means removal is in progress.
  BrowsingDataRemover* remover_;

  // The WebUI callback ID of the last performClearBrowserData request. There
  // can only be one such request in-flight.
  std::string webui_callback_id_;

  // Used to listen for pref changes to allow / disallow deleting browsing data.
  PrefChangeRegistrar profile_pref_registrar_;

  // Whether the sentence about other forms of history stored in user's account
  // should be displayed in the footer. This value is retrieved asynchronously,
  // so we cache it here.
  bool show_history_footer_;

  // Whether we should show a dialog informing the user about other forms of
  // history stored in their account after the history deletion is finished.
  bool show_history_deletion_dialog_;

  // A weak pointer factory for asynchronous calls referencing this class.
  base::WeakPtrFactory<ClearBrowsingDataHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ClearBrowsingDataHandler);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_CLEAR_BROWSING_DATA_HANDLER_H_
