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

void EnsureTabStatsMatchExpectations(const TabsStats& expected,
                                     const TabsStats& actual) {
  EXPECT_EQ(expected.total_tab_count, actual.total_tab_count);
  EXPECT_EQ(expected.total_tab_count_max, actual.total_tab_count_max);
  EXPECT_EQ(expected.max_tab_per_window, actual.max_tab_per_window);
  EXPECT_EQ(expected.window_count, actual.window_count);
  EXPECT_EQ(expected.window_count_max, actual.window_count_max);
}

}  // namespace

IN_PROC_BROWSER_TEST_F(TabStatsTrackerBrowserTest,
                       TabsAndWindowsAreCountedAccurately) {
  // Assert that the |TabStatsTracker| instance is initialized during the
  // creation of the main browser.
  ASSERT_NE(static_cast<TabStatsTracker*>(nullptr), tab_stats_tracker_);

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

}  // namespace metrics
