// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CLEAR_BROWSER_DATA_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CLEAR_BROWSER_DATA_HANDLER_H_

#include "base/prefs/pref_member.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/ui/webui/options/options_ui.h"

namespace options {

// Clear browser data handler page UI handler.
class ClearBrowserDataHandler : public OptionsPageUIHandler,
                                public BrowsingDataRemover::Observer {
 public:
  ClearBrowserDataHandler();
  virtual ~ClearBrowserDataHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings) OVERRIDE;
  virtual void InitializeHandler() OVERRIDE;
  virtual void InitializePage() OVERRIDE;

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  void UpdateInfoBannerVisibility();

 private:
  // Javascript callback to start clearing data.
  void HandleClearBrowserData(const ListValue* value);

  // BrowsingDataRemover::Observer implementation.
  // Closes the dialog once all requested data has been removed.
  virtual void OnBrowsingDataRemoverDone() OVERRIDE;

  // Updates UI when the pref to allow clearing history changes.
  virtual void OnBrowsingHistoryPrefChanged();

  // If non-null it means removal is in progress. BrowsingDataRemover takes care
  // of deleting itself when done.
  BrowsingDataRemover* remover_;

  // Keeps track of whether clearing LSO data is supported.
  BooleanPrefMember clear_plugin_lso_data_enabled_;

  // Keeps track of whether Pepper Flash is enabled and thus Flapper-specific
  // settings and removal options (e.g. Content Licenses) are available.
  BooleanPrefMember pepper_flash_settings_enabled_;

  // Keeps track of whether deleting browsing history and downloads is allowed.
  BooleanPrefMember allow_deleting_browser_history_;

  DISALLOW_COPY_AND_ASSIGN(ClearBrowserDataHandler);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CLEAR_BROWSER_DATA_HANDLER_H_
