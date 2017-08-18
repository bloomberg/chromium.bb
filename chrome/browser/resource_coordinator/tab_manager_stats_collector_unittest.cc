// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_manager_stats_collector.h"

#include <memory>

#include "base/logging.h"
#include "base/macros.h"
#include "base/test/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/resource_coordinator/background_tab_navigation_throttle.h"
#include "chrome/browser/resource_coordinator/tab_manager_web_contents_data.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using WebContents = content::WebContents;

namespace resource_coordinator {

namespace {

const char kDefaultUrl[] = "https://www.google.com";

}  // namespace

class NonResumingBackgroundTabNavigationThrottle
    : public BackgroundTabNavigationThrottle {
 public:
  explicit NonResumingBackgroundTabNavigationThrottle(
      content::NavigationHandle* handle)
      : BackgroundTabNavigationThrottle(handle) {}

  void ResumeNavigation() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NonResumingBackgroundTabNavigationThrottle);
};

class TabManagerStatsCollectorTest : public ChromeRenderViewHostTestHarness {
 protected:
  WebContents* CreateWebContents() {
    return content::TestWebContents::Create(profile(), nullptr);
  }

  std::unique_ptr<WebContents> CreateTabLoadingInBackground(
      TabManager* tab_manager) {
    std::unique_ptr<WebContents> contents(CreateTestWebContents());
    contents->WasHidden();

    std::unique_ptr<content::NavigationHandle> nav_handle(
        content::NavigationHandle::CreateNavigationHandleForTesting(
            GURL(kDefaultUrl), contents->GetMainFrame()));
    std::unique_ptr<BackgroundTabNavigationThrottle> throttle =
        base::MakeUnique<NonResumingBackgroundTabNavigationThrottle>(
            nav_handle.get());
    tab_manager->MaybeThrottleNavigation(throttle.get());

    return contents;
  }
};

TEST_F(TabManagerStatsCollectorTest, HistogramsSessionRestoreSwitchToTab) {
  const char kHistogramName[] = "TabManager.SessionRestore.SwitchToTab";

  TabManager tab_manager;
  std::unique_ptr<WebContents> tab(CreateWebContents());

  auto* data = tab_manager.GetWebContentsData(tab.get());
  auto* stats_collector = tab_manager.tab_manager_stats_collector_.get();

  base::HistogramTester histograms;
  histograms.ExpectTotalCount(kHistogramName, 0);

  data->SetTabLoadingState(TAB_IS_LOADING);
  stats_collector->RecordSwitchToTab(tab.get());
  stats_collector->RecordSwitchToTab(tab.get());

  // Nothing should happen until we're in a session restore.
  histograms.ExpectTotalCount(kHistogramName, 0);

  tab_manager.OnSessionRestoreStartedLoadingTabs();

  data->SetTabLoadingState(TAB_IS_NOT_LOADING);
  stats_collector->RecordSwitchToTab(tab.get());
  stats_collector->RecordSwitchToTab(tab.get());
  histograms.ExpectTotalCount(kHistogramName, 2);
  histograms.ExpectBucketCount(kHistogramName, TAB_IS_NOT_LOADING, 2);

  data->SetTabLoadingState(TAB_IS_LOADING);
  stats_collector->RecordSwitchToTab(tab.get());
  stats_collector->RecordSwitchToTab(tab.get());
  stats_collector->RecordSwitchToTab(tab.get());

  histograms.ExpectTotalCount(kHistogramName, 5);
  histograms.ExpectBucketCount(kHistogramName, TAB_IS_NOT_LOADING, 2);
  histograms.ExpectBucketCount(kHistogramName, TAB_IS_LOADING, 3);

  data->SetTabLoadingState(TAB_IS_LOADED);
  stats_collector->RecordSwitchToTab(tab.get());
  stats_collector->RecordSwitchToTab(tab.get());
  stats_collector->RecordSwitchToTab(tab.get());
  stats_collector->RecordSwitchToTab(tab.get());

  histograms.ExpectTotalCount(kHistogramName, 9);
  histograms.ExpectBucketCount(kHistogramName, TAB_IS_NOT_LOADING, 2);
  histograms.ExpectBucketCount(kHistogramName, TAB_IS_LOADING, 3);
  histograms.ExpectBucketCount(kHistogramName, TAB_IS_LOADED, 4);
}

TEST_F(TabManagerStatsCollectorTest,
       HistogramSessionRestoreExpectedTaskQueueingDuration) {
  TabManager tab_manager;
  auto* stats_collector = tab_manager.tab_manager_stats_collector_.get();

  base::HistogramTester histograms;
  histograms.ExpectTotalCount(
      TabManagerStatsCollector::
          kHistogramSessionRestoreForegroundTabExpectedTaskQueueingDuration,
      0);

  const base::TimeDelta kEQT = base::TimeDelta::FromMilliseconds(1);
  web_contents()->WasShown();
  ASSERT_FALSE(tab_manager.IsSessionRestoreLoadingTabs());

  // No metrics recorded because it is not in session restore.
  stats_collector->RecordExpectedTaskQueueingDuration(web_contents(), kEQT);
  histograms.ExpectTotalCount(
      TabManagerStatsCollector::
          kHistogramSessionRestoreForegroundTabExpectedTaskQueueingDuration,
      0);

  tab_manager.OnSessionRestoreStartedLoadingTabs();
  ASSERT_TRUE(tab_manager.IsSessionRestoreLoadingTabs());

  web_contents()->WasHidden();
  // No metrics recorded because the tab is background.
  stats_collector->RecordExpectedTaskQueueingDuration(web_contents(), kEQT);
  histograms.ExpectTotalCount(
      TabManagerStatsCollector::
          kHistogramSessionRestoreForegroundTabExpectedTaskQueueingDuration,
      0);

  web_contents()->WasShown();
  stats_collector->RecordExpectedTaskQueueingDuration(web_contents(), kEQT);
  histograms.ExpectTotalCount(
      TabManagerStatsCollector::
          kHistogramSessionRestoreForegroundTabExpectedTaskQueueingDuration,
      1);

  tab_manager.OnSessionRestoreFinishedLoadingTabs();
  ASSERT_FALSE(tab_manager.IsSessionRestoreLoadingTabs());

  // No metrics recorded because it is not in session restore.
  stats_collector->RecordExpectedTaskQueueingDuration(web_contents(), kEQT);
  histograms.ExpectTotalCount(
      TabManagerStatsCollector::
          kHistogramSessionRestoreForegroundTabExpectedTaskQueueingDuration,
      1);
}

TEST_F(TabManagerStatsCollectorTest,
       HistogramBackgroundTabOpeningExpectedTaskQueueingDuration) {
  // Use the global TabManager because WebContentsData::DidStopLoading affects
  // the global TabManager.
  TabManager* tab_manager = g_browser_process->GetTabManager();
  auto* stats_collector = tab_manager->tab_manager_stats_collector_.get();

  base::HistogramTester histograms;
  histograms.ExpectTotalCount(
      TabManagerStatsCollector::
          kHistogramBackgroundTabOpeningForegroundTabExpectedTaskQueueingDuration,
      0);

  const base::TimeDelta kEQT = base::TimeDelta::FromMilliseconds(1);
  web_contents()->WasShown();
  ASSERT_FALSE(tab_manager->IsLoadingBackgroundTabs());

  // No metrics recorded because there is no background tab loading.
  stats_collector->RecordExpectedTaskQueueingDuration(web_contents(), kEQT);
  histograms.ExpectTotalCount(
      TabManagerStatsCollector::
          kHistogramBackgroundTabOpeningForegroundTabExpectedTaskQueueingDuration,
      0);

  // Create a tab loading in background.
  std::unique_ptr<WebContents> contents =
      CreateTabLoadingInBackground(tab_manager);
  ASSERT_TRUE(tab_manager->IsLoadingBackgroundTabs());

  // No metrics recorded because the tab is background.
  web_contents()->WasHidden();
  stats_collector->RecordExpectedTaskQueueingDuration(web_contents(), kEQT);
  histograms.ExpectTotalCount(
      TabManagerStatsCollector::
          kHistogramBackgroundTabOpeningForegroundTabExpectedTaskQueueingDuration,
      0);

  web_contents()->WasShown();
  stats_collector->RecordExpectedTaskQueueingDuration(web_contents(), kEQT);
  histograms.ExpectTotalCount(
      TabManagerStatsCollector::
          kHistogramBackgroundTabOpeningForegroundTabExpectedTaskQueueingDuration,
      1);

  // Stop background tab loading.
  tab_manager->GetWebContentsData(contents.get())->DidStopLoading();
  ASSERT_FALSE(tab_manager->IsLoadingBackgroundTabs());

  // No metrics recorded because there is no background tab loading.
  stats_collector->RecordExpectedTaskQueueingDuration(web_contents(), kEQT);
  histograms.ExpectTotalCount(
      TabManagerStatsCollector::
          kHistogramBackgroundTabOpeningForegroundTabExpectedTaskQueueingDuration,
      1);
}

}  // namespace resource_coordinator
