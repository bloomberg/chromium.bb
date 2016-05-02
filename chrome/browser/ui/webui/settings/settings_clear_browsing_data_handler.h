// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_CLEAR_BROWSING_DATA_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_CLEAR_BROWSING_DATA_HANDLER_H_

#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/prefs/pref_member.h"

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

  // ProfileSyncService to observe sync state changes.
  ProfileSyncService* sync_service_;

  // If non-null it means removal is in progress.
  BrowsingDataRemover* remover_;

  // The WebUI callback ID of the last performClearBrowserData request. There
  // can only be one such request in-flight.
  std::string webui_callback_id_;

  // Keeps track of whether clearing LSO data is supported.
  BooleanPrefMember clear_plugin_lso_data_enabled_;

  // Keeps track of whether Pepper Flash is enabled and thus Flapper-specific
  // settings and removal options (e.g. Content Licenses) are available.
  BooleanPrefMember pepper_flash_settings_enabled_;

  // Keeps track of whether deleting browsing history and downloads is allowed.
  BooleanPrefMember allow_deleting_browser_history_;

  // Whether the sentence about other forms of history stored in user's account
  // should be displayed in the footer. This value is retrieved asynchronously,
  // so we cache it here.
  bool should_show_history_footer_;

  // A weak pointer factory for asynchronous calls referencing this class.
  base::WeakPtrFactory<ClearBrowsingDataHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ClearBrowsingDataHandler);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_CLEAR_BROWSING_DATA_HANDLER_H_
