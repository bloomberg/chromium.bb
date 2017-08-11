// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/tab_stats_tracker.h"

#include "base/message_loop/message_loop.h"
#include "base/test/histogram_tester.h"
#include "base/test/power_monitor_test_base.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using TabStatsTracker = metrics::TabStatsTracker;

class TabStatsTrackerBrowserTest : public InProcessBrowserTest {
 public:
  TabStatsTrackerBrowserTest() : tab_stats_tracker_(nullptr) {}

  void SetUpOnMainThread() override {
    tab_stats_tracker_ = TabStatsTracker::GetInstance();
  }

 protected:
  TabStatsTracker* tab_stats_tracker_;

  DISALLOW_COPY_AND_ASSIGN(TabStatsTrackerBrowserTest);
};

class TestTabStatsTracker : public TabStatsTracker {
 public:
  using UmaStatsReportingDelegate = TabStatsTracker::UmaStatsReportingDelegate;

  TestTabStatsTracker() {}
  ~TestTabStatsTracker() override {}

  size_t AddTabs(size_t tab_count) {
    total_tabs_count_ += tab_count;
    return total_tabs_count_;
  }

  size_t RemoveTabs(size_t tab_count) {
    EXPECT_LE(tab_count, total_tabs_count_);
    total_tabs_count_ -= tab_count;
    return total_tabs_count_;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestTabStatsTracker);
};

class TabStatsTrackerTest : public testing::Test {
 public:
  using UmaStatsReportingDelegate =
      TestTabStatsTracker::UmaStatsReportingDelegate;

  TabStatsTrackerTest() {
    power_monitor_source_ = new base::PowerMonitorTestSource();
    power_monitor_.reset(new base::PowerMonitor(
        std::unique_ptr<base::PowerMonitorSource>(power_monitor_source_)));
    // The tab stats tracker has to be created after the power monitor has it's
    // using it.
    tab_stats_tracker_.reset(new TestTabStatsTracker());
  }

  void TearDown() override { tab_stats_tracker_.reset(nullptr); }

  // The tabs stat tracker instance, it should be created in the SetUp
  std::unique_ptr<TestTabStatsTracker> tab_stats_tracker_;

  // Used to simulate power events.
  base::PowerMonitorTestSource* power_monitor_source_;
  std::unique_ptr<base::PowerMonitor> power_monitor_;

  // The Power Monitor requires a message loop.
  base::MessageLoop message_loop_;

  // Used to make sure that the metrics are reported properly.
  base::HistogramTester histogram_tester_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TabStatsTrackerTest);
};

// Comparator for base::Bucket values.
bool CompareHistogramBucket(const base::Bucket& l, const base::Bucket& r) {
  return l.min < r.min;
}

}  // namespace

IN_PROC_BROWSER_TEST_F(TabStatsTrackerBrowserTest,
                       TabsAndBrowsersAreCountedAccurately) {
  // Assert that the |TabStatsTracker| instance is initialized during the
  // creation of the main browser.
  ASSERT_NE(static_cast<TabStatsTracker*>(nullptr), tab_stats_tracker_);

  // There should be only one windows with one tab at startup.
  EXPECT_EQ(1U, tab_stats_tracker_->browser_count());
  EXPECT_EQ(1U, tab_stats_tracker_->total_tab_count());

  // Add a tab and make sure that the counters get updated.
  AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED);
  EXPECT_EQ(2U, tab_stats_tracker_->total_tab_count());
  browser()->tab_strip_model()->CloseWebContentsAt(1, 0);
  EXPECT_EQ(1U, tab_stats_tracker_->total_tab_count());

  Browser* browser = CreateBrowser(ProfileManager::GetActiveUserProfile());
  EXPECT_EQ(2U, tab_stats_tracker_->browser_count());

  AddTabAtIndexToBrowser(browser, 1, GURL("about:blank"),
                         ui::PAGE_TRANSITION_TYPED, true);
  EXPECT_EQ(3U, tab_stats_tracker_->total_tab_count());

  CloseBrowserSynchronously(browser);
  EXPECT_EQ(1U, tab_stats_tracker_->browser_count());
  EXPECT_EQ(1U, tab_stats_tracker_->total_tab_count());
}

TEST_F(TabStatsTrackerTest, OnResume) {
  // Makes sure that there's no sample initially.
  histogram_tester_.ExpectTotalCount(
      UmaStatsReportingDelegate::kNumberOfTabsOnResumeHistogramName, 0);

  // Creates some tabs.
  size_t expected_tab_count = tab_stats_tracker_->AddTabs(12);

  std::vector<base::Bucket> count_buckets;
  count_buckets.push_back(base::Bucket(expected_tab_count, 1));

  // Generates a resume event that should end up calling the
  // |ReportTabCountOnResume| method of the reporting delegate.
  power_monitor_source_->GenerateSuspendEvent();
  power_monitor_source_->GenerateResumeEvent();

  // There should be only one sample for the |kNumberOfTabsOnResume| histogram.
  histogram_tester_.ExpectTotalCount(
      UmaStatsReportingDelegate::kNumberOfTabsOnResumeHistogramName,
      count_buckets.size());
  EXPECT_EQ(histogram_tester_.GetAllSamples(
                UmaStatsReportingDelegate::kNumberOfTabsOnResumeHistogramName),
            count_buckets);

  // Removes some tabs and update the expectations.
  expected_tab_count = tab_stats_tracker_->RemoveTabs(5);
  count_buckets.push_back(base::Bucket(expected_tab_count, 1));
  std::sort(count_buckets.begin(), count_buckets.end(), CompareHistogramBucket);

  // Generates another resume event.
  power_monitor_source_->GenerateSuspendEvent();
  power_monitor_source_->GenerateResumeEvent();

  // There should be 2 samples for this metric now.
  histogram_tester_.ExpectTotalCount(
      UmaStatsReportingDelegate::kNumberOfTabsOnResumeHistogramName,
      count_buckets.size());
  EXPECT_EQ(histogram_tester_.GetAllSamples(
                UmaStatsReportingDelegate::kNumberOfTabsOnResumeHistogramName),
            count_buckets);
}
