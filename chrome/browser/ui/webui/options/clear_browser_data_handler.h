// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CLEAR_BROWSER_DATA_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CLEAR_BROWSER_DATA_HANDLER_H_

#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/browsing_data/browsing_data_counter.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/prefs/pref_member.h"

namespace options {

// Clear browser data handler page UI handler.
class ClearBrowserDataHandler : public OptionsPageUIHandler,
                                public BrowsingDataRemover::Observer,
                                public sync_driver::SyncServiceObserver {
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
  void AddCounter(scoped_ptr<BrowsingDataCounter> counter);

  // Updates a counter in the UI according to the |result|.
  void UpdateCounterText(scoped_ptr<BrowsingDataCounter::Result> result);

  // Implementation of SyncServiceObserver. Updates the support string at the
  // bottom of the dialog.
  void OnStateChanged() override;

  // If non-null it means removal is in progress.
  BrowsingDataRemover* remover_;

  // Keeps track of whether clearing LSO data is supported.
  BooleanPrefMember clear_plugin_lso_data_enabled_;

  // Keeps track of whether Pepper Flash is enabled and thus Flapper-specific
  // settings and removal options (e.g. Content Licenses) are available.
  BooleanPrefMember pepper_flash_settings_enabled_;

  // Keeps track of whether deleting browsing history and downloads is allowed.
  BooleanPrefMember allow_deleting_browser_history_;

  // Counters that calculate the data volume for some of the data types.
  ScopedVector<BrowsingDataCounter> counters_;

  // Informs us whether the user is syncing their data.
  ProfileSyncService* sync_service_;

  DISALLOW_COPY_AND_ASSIGN(ClearBrowserDataHandler);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CLEAR_BROWSER_DATA_HANDLER_H_
