// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/tab_stats_tracker.h"

#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

namespace {

using TabsStats = TabStatsDataStore::TabsStats;

void EnsureTabStatsMatchExpectations(const TabsStats& expected,
                                     const TabsStats& actual) {
  EXPECT_EQ(expected.total_tab_count, actual.total_tab_count);
  EXPECT_EQ(expected.total_tab_count_max, actual.total_tab_count_max);
  EXPECT_EQ(expected.max_tab_per_window, actual.max_tab_per_window);
  EXPECT_EQ(expected.window_count, actual.window_count);
  EXPECT_EQ(expected.window_count_max, actual.window_count_max);
}

}  // namespace

class TabStatsTrackerBrowserTest : public InProcessBrowserTest {
 public:
  TabStatsTrackerBrowserTest() : tab_stats_tracker_(nullptr) {}

  void SetUpOnMainThread() override {
    tab_stats_tracker_ = TabStatsTracker::GetInstance();
    ASSERT_TRUE(tab_stats_tracker_ != nullptr);
  }

 protected:
  TabStatsTracker* tab_stats_tracker_;

  DISALLOW_COPY_AND_ASSIGN(TabStatsTrackerBrowserTest);
};

IN_PROC_BROWSER_TEST_F(TabStatsTrackerBrowserTest,
                       TabsAndWindowsAreCountedAccurately) {
  // Assert that the |TabStatsTracker| instance is initialized during the
  // creation of the main browser.
  ASSERT_TRUE(tab_stats_tracker_ != nullptr);

  TabsStats expected_stats = {};

  // There should be only one window with one tab at startup.
  expected_stats.total_tab_count = 1;
  expected_stats.total_tab_count_max = 1;
  expected_stats.max_tab_per_window = 1;
  expected_stats.window_count = 1;
  expected_stats.window_count_max = 1;

  EnsureTabStatsMatchExpectations(expected_stats,
                                  tab_stats_tracker_->tab_stats());

  // Add a tab and make sure that the counters get updated.
  AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED);
  ++expected_stats.total_tab_count;
  ++expected_stats.total_tab_count_max;
  ++expected_stats.max_tab_per_window;
  EnsureTabStatsMatchExpectations(expected_stats,
                                  tab_stats_tracker_->tab_stats());

  browser()->tab_strip_model()->CloseWebContentsAt(1, 0);
  --expected_stats.total_tab_count;
  EnsureTabStatsMatchExpectations(expected_stats,
                                  tab_stats_tracker_->tab_stats());

  Browser* browser = CreateBrowser(ProfileManager::GetActiveUserProfile());
  ++expected_stats.total_tab_count;
  ++expected_stats.window_count;
  ++expected_stats.window_count_max;
  EnsureTabStatsMatchExpectations(expected_stats,
                                  tab_stats_tracker_->tab_stats());

  AddTabAtIndexToBrowser(browser, 1, GURL("about:blank"),
                         ui::PAGE_TRANSITION_TYPED, true);
  ++expected_stats.total_tab_count;
  ++expected_stats.total_tab_count_max;
  EnsureTabStatsMatchExpectations(expected_stats,
                                  tab_stats_tracker_->tab_stats());

  CloseBrowserSynchronously(browser);
  expected_stats.total_tab_count = 1;
  expected_stats.window_count = 1;
  EnsureTabStatsMatchExpectations(expected_stats,
                                  tab_stats_tracker_->tab_stats());
}

IN_PROC_BROWSER_TEST_F(TabStatsTrackerBrowserTest,
                       TabDeletionGetsHandledProperly) {
  // Assert that the |TabStatsTracker| instance is initialized during the
  // creation of the main browser.
  ASSERT_TRUE(tab_stats_tracker_ != nullptr);

  constexpr base::TimeDelta kValidLongInterval = base::TimeDelta::FromHours(12);

  TabStatsDataStore* data_store = tab_stats_tracker_->tab_stats_data_store();
  TabStatsDataStore::TabsStateDuringIntervalMap* interval_map =
      data_store->AddInterval();

  AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED);

  EXPECT_EQ(2U, interval_map->size());

  content::WebContents* web_contents =
      data_store->existing_tabs_for_testing()->begin()->first;

  // Delete one of the WebContents without calling the |OnTabRemoved| function,
  // the WebContentsObserver owned by |tab_stats_tracker_| should be notified
  // and this should be handled correctly.
  TabStatsDataStore::TabID tab_id =
      data_store->GetTabIDForTesting(web_contents).value();
  delete web_contents;
  EXPECT_TRUE(base::ContainsKey(*interval_map, tab_id));
  tab_stats_tracker_->OnInterval(kValidLongInterval, interval_map);
  EXPECT_EQ(1U, interval_map->size());
  EXPECT_FALSE(base::ContainsKey(*interval_map, tab_id));

  web_contents = data_store->existing_tabs_for_testing()->begin()->first;

  // Do this a second time, ensures that the situation where there's no existing
  // tabs is handled properly.
  tab_id = data_store->GetTabIDForTesting(web_contents).value();
  delete web_contents;
  EXPECT_TRUE(base::ContainsKey(*interval_map, tab_id));
  tab_stats_tracker_->OnInterval(kValidLongInterval, interval_map);
  EXPECT_EQ(0U, interval_map->size());
}

}  // namespace metrics
