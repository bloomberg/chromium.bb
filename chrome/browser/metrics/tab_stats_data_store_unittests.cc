// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/tab_stats_data_store.h"

#include "chrome/browser/metrics/tab_stats_tracker.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

namespace {

using TabsStats = TabStatsDataStore::TabsStats;

using TabStatsDataStoreTest = testing::Test;

}  // namespace

TEST_F(TabStatsDataStoreTest, TabStatsGetsReloadedFromLocalState) {
  // This tests creates add some tabs/windows to a data store instance and then
  // reinitializes it (and so the count of active tabs/windows drops to zero).
  // As the TabStatsTracker constructor restores its state from the pref service
  // the maximums should be restored.
  TestingPrefServiceSimple pref_service;
  TabStatsTracker::RegisterPrefs(pref_service.registry());
  std::unique_ptr<TabStatsDataStore> data_store(
      std::make_unique<TabStatsDataStore>(&pref_service));
  size_t expected_tab_count = 12;
  data_store->OnTabsAdded(expected_tab_count);
  size_t expected_window_count = 5;
  for (size_t i = 0; i < expected_window_count; ++i)
    data_store->OnWindowAdded();
  size_t expected_max_tab_per_window = expected_tab_count - 1;
  data_store->UpdateMaxTabsPerWindowIfNeeded(expected_max_tab_per_window);

  TabsStats stats = data_store->tab_stats();
  EXPECT_EQ(expected_tab_count, stats.total_tab_count_max);
  EXPECT_EQ(expected_max_tab_per_window, stats.max_tab_per_window);
  EXPECT_EQ(expected_window_count, stats.window_count_max);

  // Reset the |tab_stats_tracker_| and ensure that the maximums are restored.
  data_store.reset(new TabStatsDataStore(&pref_service));

  TabsStats stats2 = data_store->tab_stats();
  EXPECT_EQ(stats.total_tab_count_max, stats2.total_tab_count_max);
  EXPECT_EQ(stats.max_tab_per_window, stats2.max_tab_per_window);
  EXPECT_EQ(stats.window_count_max, stats2.window_count_max);
  // The actual number of tabs/window should be 0 as it's not stored in the
  // pref service.
  EXPECT_EQ(0U, stats2.window_count);
  EXPECT_EQ(0U, stats2.total_tab_count);
}

}  // namespace metrics
