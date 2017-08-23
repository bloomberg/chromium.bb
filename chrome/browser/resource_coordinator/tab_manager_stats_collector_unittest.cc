// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_manager_stats_collector.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/test/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/resource_coordinator/tab_manager_web_contents_data.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

using WebContents = content::WebContents;

namespace resource_coordinator {

class TabManagerStatsCollectorTest
    : public ChromeRenderViewHostTestHarness,
      public testing::WithParamInterface<
          std::pair<bool,   // should_test_session_restore
                    bool>>  // should_test_background_tab_opening
{
 protected:
  TabManagerStatsCollectorTest() = default;
  ~TabManagerStatsCollectorTest() override = default;

  TabManagerStatsCollector* tab_manager_stats_collector() {
    return tab_manager_.tab_manager_stats_collector_.get();
  }

  void StartSessionRestore() {
    tab_manager_stats_collector()->OnSessionRestoreStartedLoadingTabs();
  }

  void FinishSessionRestore() {
    tab_manager_stats_collector()->OnSessionRestoreFinishedLoadingTabs();
  }

  void StartBackgroundTabOpeningSession() {
    tab_manager_stats_collector()->OnBackgroundTabOpeningSessionStarted();
  }

  void FinishBackgroundTabOpeningSession() {
    tab_manager_stats_collector()->OnBackgroundTabOpeningSessionEnded();
  }

  TabManager::WebContentsData* GetWebContentsData(WebContents* contents) const {
    return tab_manager_.GetWebContentsData(contents);
  }

  void SetUp() override {
    std::tie(should_test_session_restore_,
             should_test_background_tab_opening_) = GetParam();
    ChromeRenderViewHostTestHarness::SetUp();
  }

 protected:
  base::HistogramTester histogram_tester_;
  bool should_test_session_restore_;
  bool should_test_background_tab_opening_;

 private:
  TabManager tab_manager_;

  DISALLOW_COPY_AND_ASSIGN(TabManagerStatsCollectorTest);
};

class TabManagerStatsCollectorTabSwitchTest
    : public TabManagerStatsCollectorTest {
 protected:
  TabManagerStatsCollectorTabSwitchTest() = default;
  ~TabManagerStatsCollectorTabSwitchTest() override = default;

  void SetForegroundTabLoadingState(TabLoadingState state) {
    GetWebContentsData(foreground_tab_)->SetTabLoadingState(state);
  }

  void SetBackgroundTabLoadingState(TabLoadingState state) {
    GetWebContentsData(background_tab_)->SetTabLoadingState(state);
  }

  // Creating and destroying the WebContentses need to be done in the test
  // scope.
  void SetForegroundAndBackgroundTabs(WebContents* foreground_tab,
                                      WebContents* background_tab) {
    foreground_tab_ = foreground_tab;
    foreground_tab_->WasShown();
    background_tab_ = background_tab;
    background_tab_->WasHidden();
  }

  void FinishLoadingForegroundTab() {
    SetForegroundTabLoadingState(TAB_IS_LOADED);
    tab_manager_stats_collector()->OnDidStopLoading(foreground_tab_);
  }

  void FinishLoadingBackgroundTab() {
    SetBackgroundTabLoadingState(TAB_IS_LOADED);
    tab_manager_stats_collector()->OnDidStopLoading(background_tab_);
  }

  void SwitchToBackgroundTab() {
    tab_manager_stats_collector()->RecordSwitchToTab(foreground_tab_,
                                                     background_tab_);
    SetForegroundAndBackgroundTabs(background_tab_, foreground_tab_);
  }

 private:
  WebContents* foreground_tab_;
  WebContents* background_tab_;

  DISALLOW_COPY_AND_ASSIGN(TabManagerStatsCollectorTabSwitchTest);
};

TEST_P(TabManagerStatsCollectorTabSwitchTest, HistogramsSwitchToTab) {
  struct HistogramParameters {
    const char* histogram_name;
    bool enabled;
  };
  std::vector<HistogramParameters> histogram_parameters = {
      HistogramParameters{
          TabManagerStatsCollector::kHistogramSessionRestoreSwitchToTab,
          should_test_session_restore_},
      HistogramParameters{
          TabManagerStatsCollector::kHistogramBackgroundTabOpeningSwitchToTab,
          should_test_background_tab_opening_},
  };

  std::unique_ptr<WebContents> tab1(CreateTestWebContents());
  std::unique_ptr<WebContents> tab2(CreateTestWebContents());
  SetForegroundAndBackgroundTabs(tab1.get(), tab2.get());

  if (should_test_session_restore_)
    StartSessionRestore();
  if (should_test_background_tab_opening_)
    StartBackgroundTabOpeningSession();

  SetBackgroundTabLoadingState(TAB_IS_NOT_LOADING);
  SetForegroundTabLoadingState(TAB_IS_NOT_LOADING);
  SwitchToBackgroundTab();
  SwitchToBackgroundTab();
  for (const auto& param : histogram_parameters) {
    if (param.enabled) {
      histogram_tester_.ExpectTotalCount(param.histogram_name, 2);
      histogram_tester_.ExpectBucketCount(param.histogram_name,
                                          TAB_IS_NOT_LOADING, 2);
    } else {
      histogram_tester_.ExpectTotalCount(param.histogram_name, 0);
    }
  }

  SetBackgroundTabLoadingState(TAB_IS_LOADING);
  SetForegroundTabLoadingState(TAB_IS_LOADING);
  SwitchToBackgroundTab();
  SwitchToBackgroundTab();
  SwitchToBackgroundTab();
  for (const auto& param : histogram_parameters) {
    if (param.enabled) {
      histogram_tester_.ExpectTotalCount(param.histogram_name, 5);
      histogram_tester_.ExpectBucketCount(param.histogram_name,
                                          TAB_IS_NOT_LOADING, 2);
      histogram_tester_.ExpectBucketCount(param.histogram_name, TAB_IS_LOADING,
                                          3);
    } else {
      histogram_tester_.ExpectTotalCount(param.histogram_name, 0);
    }
  }

  SetBackgroundTabLoadingState(TAB_IS_LOADED);
  SetForegroundTabLoadingState(TAB_IS_LOADED);
  SwitchToBackgroundTab();
  SwitchToBackgroundTab();
  SwitchToBackgroundTab();
  SwitchToBackgroundTab();
  for (const auto& param : histogram_parameters) {
    if (param.enabled) {
      histogram_tester_.ExpectTotalCount(param.histogram_name, 9);
      histogram_tester_.ExpectBucketCount(param.histogram_name,
                                          TAB_IS_NOT_LOADING, 2);
      histogram_tester_.ExpectBucketCount(param.histogram_name, TAB_IS_LOADING,
                                          3);
      histogram_tester_.ExpectBucketCount(param.histogram_name, TAB_IS_LOADED,
                                          4);
    } else {
      histogram_tester_.ExpectTotalCount(param.histogram_name, 0);
    }
  }
}

TEST_P(TabManagerStatsCollectorTabSwitchTest, HistogramsTabSwitchLoadTime) {
  std::unique_ptr<WebContents> tab1(CreateTestWebContents());
  std::unique_ptr<WebContents> tab2(CreateTestWebContents());
  SetForegroundAndBackgroundTabs(tab1.get(), tab2.get());

  if (should_test_session_restore_)
    StartSessionRestore();
  if (should_test_background_tab_opening_)
    StartBackgroundTabOpeningSession();

  SetBackgroundTabLoadingState(TAB_IS_NOT_LOADING);
  SetForegroundTabLoadingState(TAB_IS_LOADED);
  SwitchToBackgroundTab();
  FinishLoadingForegroundTab();
  histogram_tester_.ExpectTotalCount(
      TabManagerStatsCollector::kHistogramSessionRestoreTabSwitchLoadTime,
      should_test_session_restore_ ? 1 : 0);
  histogram_tester_.ExpectTotalCount(
      TabManagerStatsCollector::kHistogramBackgroundTabOpeningTabSwitchLoadTime,
      should_test_background_tab_opening_ ? 1 : 0);

  SetBackgroundTabLoadingState(TAB_IS_LOADING);
  SwitchToBackgroundTab();
  FinishLoadingForegroundTab();
  histogram_tester_.ExpectTotalCount(
      TabManagerStatsCollector::kHistogramSessionRestoreTabSwitchLoadTime,
      should_test_session_restore_ ? 2 : 0);
  histogram_tester_.ExpectTotalCount(
      TabManagerStatsCollector::kHistogramBackgroundTabOpeningTabSwitchLoadTime,
      should_test_background_tab_opening_ ? 2 : 0);

  // Metrics aren't recorded when the foreground tab has not finished loading
  // and the user switches to a different tab.
  SetBackgroundTabLoadingState(TAB_IS_LOADING);
  SetForegroundTabLoadingState(TAB_IS_LOADED);
  SwitchToBackgroundTab();
  // Foreground tab is currently loading and being tracked.
  SwitchToBackgroundTab();
  // The previous foreground tab is no longer tracked.
  FinishLoadingBackgroundTab();
  SwitchToBackgroundTab();
  histogram_tester_.ExpectTotalCount(
      TabManagerStatsCollector::kHistogramSessionRestoreTabSwitchLoadTime,
      should_test_session_restore_ ? 2 : 0);
  histogram_tester_.ExpectTotalCount(
      TabManagerStatsCollector::kHistogramBackgroundTabOpeningTabSwitchLoadTime,
      should_test_background_tab_opening_ ? 2 : 0);

  // The count shouldn't change when we're no longer in a session restore or
  // background tab opening.
  if (should_test_session_restore_)
    FinishSessionRestore();
  if (should_test_background_tab_opening_)
    FinishBackgroundTabOpeningSession();
  SetBackgroundTabLoadingState(TAB_IS_NOT_LOADING);
  SetForegroundTabLoadingState(TAB_IS_LOADED);
  SwitchToBackgroundTab();
  FinishLoadingForegroundTab();
  histogram_tester_.ExpectTotalCount(
      TabManagerStatsCollector::kHistogramSessionRestoreTabSwitchLoadTime,
      should_test_session_restore_ ? 2 : 0);
  histogram_tester_.ExpectTotalCount(
      TabManagerStatsCollector::kHistogramBackgroundTabOpeningTabSwitchLoadTime,
      should_test_background_tab_opening_ ? 2 : 0);
}

TEST_P(TabManagerStatsCollectorTest, HistogramsExpectedTaskQueueingDuration) {
  auto* stats_collector = tab_manager_stats_collector();

  const base::TimeDelta kEQT = base::TimeDelta::FromMilliseconds(1);
  web_contents()->WasShown();

  // No metrics recorded because there is no session restore or background tab
  // opening.
  stats_collector->RecordExpectedTaskQueueingDuration(web_contents(), kEQT);
  histogram_tester_.ExpectTotalCount(
      TabManagerStatsCollector::
          kHistogramSessionRestoreForegroundTabExpectedTaskQueueingDuration,
      0);
  histogram_tester_.ExpectTotalCount(
      TabManagerStatsCollector::
          kHistogramBackgroundTabOpeningForegroundTabExpectedTaskQueueingDuration,
      0);

  if (should_test_session_restore_)
    StartSessionRestore();
  if (should_test_background_tab_opening_)
    StartBackgroundTabOpeningSession();

  // No metrics recorded because the tab is background.
  web_contents()->WasHidden();
  stats_collector->RecordExpectedTaskQueueingDuration(web_contents(), kEQT);
  histogram_tester_.ExpectTotalCount(
      TabManagerStatsCollector::
          kHistogramSessionRestoreForegroundTabExpectedTaskQueueingDuration,
      0);
  histogram_tester_.ExpectTotalCount(
      TabManagerStatsCollector::
          kHistogramBackgroundTabOpeningForegroundTabExpectedTaskQueueingDuration,
      0);

  web_contents()->WasShown();
  stats_collector->RecordExpectedTaskQueueingDuration(web_contents(), kEQT);
  histogram_tester_.ExpectTotalCount(
      TabManagerStatsCollector::
          kHistogramSessionRestoreForegroundTabExpectedTaskQueueingDuration,
      should_test_session_restore_ ? 1 : 0);
  histogram_tester_.ExpectTotalCount(
      TabManagerStatsCollector::
          kHistogramBackgroundTabOpeningForegroundTabExpectedTaskQueueingDuration,
      should_test_background_tab_opening_ ? 1 : 0);

  if (should_test_session_restore_)
    FinishSessionRestore();
  if (should_test_background_tab_opening_)
    FinishBackgroundTabOpeningSession();

  // No metrics recorded because there is no session restore or background tab
  // opening.
  stats_collector->RecordExpectedTaskQueueingDuration(web_contents(), kEQT);
  histogram_tester_.ExpectTotalCount(
      TabManagerStatsCollector::
          kHistogramSessionRestoreForegroundTabExpectedTaskQueueingDuration,
      should_test_session_restore_ ? 1 : 0);
  histogram_tester_.ExpectTotalCount(
      TabManagerStatsCollector::
          kHistogramBackgroundTabOpeningForegroundTabExpectedTaskQueueingDuration,
      should_test_background_tab_opening_ ? 1 : 0);
}

INSTANTIATE_TEST_CASE_P(
    ,
    TabManagerStatsCollectorTabSwitchTest,
    ::testing::Values(std::make_pair(false,   // Session restore
                                     false),  // Background tab opening
                      std::make_pair(true, false),
                      std::make_pair(false, true),
                      std::make_pair(true, true)));
INSTANTIATE_TEST_CASE_P(
    ,
    TabManagerStatsCollectorTest,
    ::testing::Values(std::make_pair(false,   // Session restore
                                     false),  // Background tab opening
                      std::make_pair(true, false),
                      std::make_pair(false, true),
                      std::make_pair(true, true)));

}  // namespace resource_coordinator
