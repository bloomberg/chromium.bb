// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/tab_stats_data_store.h"

#include <algorithm>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace metrics {

TabStatsDataStore::TabsStats::TabsStats()
    : total_tab_count(0U),
      total_tab_count_max(0U),
      max_tab_per_window(0U),
      window_count(0U),
      window_count_max(0U) {}

TabStatsDataStore::TabStatsDataStore(PrefService* pref_service)
    : pref_service_(pref_service) {
  DCHECK(pref_service != nullptr);
  tab_stats_.total_tab_count_max =
      pref_service->GetInteger(prefs::kTabStatsTotalTabCountMax);
  tab_stats_.max_tab_per_window =
      pref_service->GetInteger(prefs::kTabStatsMaxTabsPerWindow);
  tab_stats_.window_count_max =
      pref_service->GetInteger(prefs::kTabStatsWindowCountMax);
}

void TabStatsDataStore::OnWindowAdded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  tab_stats_.window_count++;
  UpdateWindowCountMaxIfNeeded();
}

void TabStatsDataStore::OnWindowRemoved() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(tab_stats_.window_count, 0U);
  tab_stats_.window_count--;
}

void TabStatsDataStore::OnTabsAdded(size_t tab_count) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  tab_stats_.total_tab_count += tab_count;
  UpdateTotalTabCountMaxIfNeeded();
}

void TabStatsDataStore::OnTabsRemoved(size_t tab_count) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_LE(tab_count, tab_stats_.total_tab_count);
  tab_stats_.total_tab_count -= tab_count;
}

void TabStatsDataStore::UpdateMaxTabsPerWindowIfNeeded(size_t value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (value <= tab_stats_.max_tab_per_window)
    return;
  tab_stats_.max_tab_per_window = value;
  pref_service_->SetInteger(prefs::kTabStatsMaxTabsPerWindow, value);
}

void TabStatsDataStore::ResetMaximumsToCurrentState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Set the maximums to 0 and call the Update* functions to reset it to the
  // current value and update the pref registry.
  tab_stats_.max_tab_per_window = 0;
  tab_stats_.window_count_max = 0;
  tab_stats_.total_tab_count_max = 0;
  UpdateTotalTabCountMaxIfNeeded();
  UpdateWindowCountMaxIfNeeded();

  // Iterates over the list of browsers to find the one with the maximum number
  // of tabs opened.
  BrowserList* browser_list = BrowserList::GetInstance();
  for (Browser* browser : *browser_list) {
    UpdateMaxTabsPerWindowIfNeeded(
        static_cast<size_t>(browser->tab_strip_model()->count()));
  }
}

void TabStatsDataStore::UpdateTotalTabCountMaxIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (tab_stats_.total_tab_count <= tab_stats_.total_tab_count_max)
    return;
  tab_stats_.total_tab_count_max = tab_stats_.total_tab_count;
  pref_service_->SetInteger(prefs::kTabStatsTotalTabCountMax,
                            tab_stats_.total_tab_count_max);
}

void TabStatsDataStore::UpdateWindowCountMaxIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (tab_stats_.window_count <= tab_stats_.window_count_max)
    return;
  tab_stats_.window_count_max = tab_stats_.window_count;
  pref_service_->SetInteger(prefs::kTabStatsWindowCountMax,
                            tab_stats_.window_count_max);
}

}  // namespace metrics
