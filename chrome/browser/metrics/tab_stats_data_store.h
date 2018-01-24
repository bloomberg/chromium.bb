// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_TAB_STATS_DATA_STORE_H_
#define CHROME_BROWSER_METRICS_TAB_STATS_DATA_STORE_H_

#include <memory>

#include "base/sequence_checker.h"

class PrefService;

namespace metrics {

// The data store that keeps track of all the information that the
// TabStatsTracker wants to track.
class TabStatsDataStore {
 public:
  // Houses all of the statistics gathered by the TabStatsTracker.
  struct TabsStats {
    // Constructor, initializes everything to zero.
    TabsStats();

    // The total number of tabs existing across all the windows.
    size_t total_tab_count;

    // The maximum total number of tabs that have existed across all the
    // windows at the same time.
    size_t total_tab_count_max;

    // The maximum total number of tabs that have existed at the same time in
    // a single window.
    size_t max_tab_per_window;

    // The total number of windows currently existing.
    size_t window_count;

    // The maximum total number of windows existing at the same time.
    size_t window_count_max;
  };

  explicit TabStatsDataStore(PrefService* pref_service);
  ~TabStatsDataStore() {}

  // Functions used to update the window/tab count.
  void OnWindowAdded();
  void OnWindowRemoved();
  void OnTabsAdded(size_t tab_count);
  void OnTabsRemoved(size_t tab_count);

  // Update the maximum number of tabs in a single window if |value| exceeds
  // this.
  // TODO(sebmarchand): Store a list of windows in this class and track the
  // number of tabs per window.
  void UpdateMaxTabsPerWindowIfNeeded(size_t value);

  // Reset all the maximum values to the current state, to be used once the
  // metrics have been reported.
  void ResetMaximumsToCurrentState();

  const TabsStats& tab_stats() const { return tab_stats_; }

 protected:
  // Update the maximums metrics if needed.
  void UpdateTotalTabCountMaxIfNeeded();
  void UpdateWindowCountMaxIfNeeded();

 private:
  // The tabs stats.
  TabsStats tab_stats_;

  // A raw pointer to the PrefService used to read and write the statistics.
  PrefService* pref_service_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(TabStatsDataStore);
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_TAB_STATS_DATA_STORE_H_
