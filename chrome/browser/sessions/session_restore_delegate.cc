// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_restore_delegate.h"

#include "base/metrics/field_trial.h"
#include "chrome/browser/sessions/session_restore_stats_collector.h"
#include "chrome/browser/sessions/tab_loader.h"
#include "components/favicon/content/content_favicon_driver.h"

// static
void SessionRestoreDelegate::RestoreTabs(
    const std::vector<RestoredTab>& tabs,
    const base::TimeTicks& restore_started) {
  // TODO(georgesak): make tests aware of that behavior so that they succeed if
  // tab loading is disabled.
  base::FieldTrial* trial =
      base::FieldTrialList::Find("SessionRestoreBackgroundLoading");
  bool active_only = true;
  if (!trial || trial->group_name() == "Restore") {
    TabLoader::RestoreTabs(tabs, restore_started);
    active_only = false;
  } else {
    // If we are not loading inactive tabs, restore their favicons (title has
    // already been set by now).
    for (auto& tab : tabs) {
      if (!tab.is_active) {
        favicon::ContentFaviconDriver* favicon_driver =
            favicon::ContentFaviconDriver::FromWebContents(tab.contents);
        favicon_driver->FetchFavicon(favicon_driver->GetActiveURL());
      }
    }
  }
  SessionRestoreStatsCollector::TrackTabs(tabs, restore_started, active_only);
}
