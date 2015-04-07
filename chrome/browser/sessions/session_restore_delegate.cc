// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_restore_delegate.h"

#include "base/metrics/field_trial.h"
#include "chrome/browser/sessions/session_restore_stats_collector.h"
#include "chrome/browser/sessions/tab_loader.h"

// static
void SessionRestoreDelegate::RestoreTabs(
    const std::vector<RestoredTab>& tabs,
    const base::TimeTicks& restore_started) {
  // TODO(georgesak): make tests aware of that behavior so that they succeed if
  // tab loading is disabled.
  base::FieldTrial* trial =
      base::FieldTrialList::Find("SessionRestoreBackgroundLoading");
  bool active_only = true;
  if (!trial || trial->group_name() == "Enabled") {
    TabLoader::RestoreTabs(tabs, restore_started);
    active_only = false;
  }
  SessionRestoreStatsCollector::TrackTabs(tabs, restore_started, active_only);
}
