// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_CLEAR_BROWSING_DATA_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_CLEAR_BROWSING_DATA_HANDLER_H_

#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/settings/md_settings_ui.h"
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
                                 public BrowsingDataRemover::Observer {
 public:
  explicit ClearBrowsingDataHandler(content::WebUI* webui);
  ~ClearBrowsingDataHandler() override;

  // OptionsPageUIHandler:
  void RegisterMessages() override;

 private:
  // Javascript callback to start clearing data.
  void HandleClearBrowserData(const base::ListValue* value);

  // BrowsingDataRemover::Observer implementation.
  // Re-enables clear button once all requested data has been removed.
  void OnBrowsingDataRemoverDone() override;

  // Updates UI when the pref to allow clearing history changes.
  virtual void OnBrowsingHistoryPrefChanged();

  // If non-null it means removal is in progress.
  BrowsingDataRemover* remover_;

  // Keeps track of whether clearing LSO data is supported.
  BooleanPrefMember clear_plugin_lso_data_enabled_;

  // Keeps track of whether Pepper Flash is enabled and thus Flapper-specific
  // settings and removal options (e.g. Content Licenses) are available.
  BooleanPrefMember pepper_flash_settings_enabled_;

  // Keeps track of whether deleting browsing history and downloads is allowed.
  BooleanPrefMember allow_deleting_browser_history_;

  DISALLOW_COPY_AND_ASSIGN(ClearBrowsingDataHandler);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_CLEAR_BROWSING_DATA_HANDLER_H_
