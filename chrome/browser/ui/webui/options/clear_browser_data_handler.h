// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CLEAR_BROWSER_DATA_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CLEAR_BROWSER_DATA_HANDLER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/browsing_data/core/counters/browsing_data_counter.h"
#include "components/prefs/pref_member.h"

namespace options {

// Clear browser data handler page UI handler.
class ClearBrowserDataHandler : public OptionsPageUIHandler,
                                public BrowsingDataRemover::Observer,
                                public syncer::SyncServiceObserver {
 public:
  ClearBrowserDataHandler();
  ~ClearBrowserDataHandler() override;

  // OptionsPageUIHandler implementation.
  void GetLocalizedValues(base::DictionaryValue* localized_strings) override;
  void InitializeHandler() override;
  void InitializePage() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  void UpdateInfoBannerVisibility();

 private:
  // Javascript callback for when the CBD dialog is opened. The caller does
  // not provide any parameters, so |value| is unused.
  void OnPageOpened(const base::ListValue* value);

  // Javascript callback to start clearing data.
  void HandleClearBrowserData(const base::ListValue* value);

  // BrowsingDataRemover::Observer implementation.
  // Closes the dialog once all requested data has been removed.
  void OnBrowsingDataRemoverDone() override;

  // Updates UI when the pref to allow clearing history changes.
  virtual void OnBrowsingHistoryPrefChanged();

  // Adds a |counter| for browsing data.
  void AddCounter(std::unique_ptr<browsing_data::BrowsingDataCounter> counter);

  // Updates a counter in the UI according to the |result|.
  void UpdateCounterText(
      std::unique_ptr<browsing_data::BrowsingDataCounter::Result> result);

  // Implementation of SyncServiceObserver.
  void OnStateChanged(syncer::SyncService* sync) override;

  // Updates the support string at the bottom of the dialog.
  void UpdateSyncState();

  // Finds out whether we should show a notice informing the user about other
  // forms of browsing history. Responds with an asynchronous callback to
  // |UpdateHistoryNotice|.
  void RefreshHistoryNotice();

  // Shows or hides the notice about other forms of browsing history.
  void UpdateHistoryNotice(bool show);

  // Remembers whether we should popup a dialog about other forms of browsing
  // history when the user deletes the history for the first time.
  void UpdateHistoryDeletionDialog(bool show);

  // If non-null it means removal is in progress.
  BrowsingDataRemover* remover_;

  // Keeps track of whether clearing LSO data is supported.
  BooleanPrefMember clear_plugin_lso_data_enabled_;

  // Keeps track of whether deleting browsing history and downloads is allowed.
  BooleanPrefMember allow_deleting_browser_history_;

  // Counters that calculate the data volume for some of the data types.
  std::vector<std::unique_ptr<browsing_data::BrowsingDataCounter>> counters_;

  // Informs us whether the user is syncing their data.
  browser_sync::ProfileSyncService* sync_service_;

  // Whether we should show a notice about other forms of browsing history.
  bool should_show_history_notice_;

  // Whether we should popup a dialog about other forms of browsing history
  // when the user deletes their browsing history for the first time.
  bool should_show_history_deletion_dialog_;

  // A weak pointer factory for asynchronous calls referencing this class.
  base::WeakPtrFactory<ClearBrowserDataHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ClearBrowserDataHandler);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CLEAR_BROWSER_DATA_HANDLER_H_
