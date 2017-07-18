// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_manager_stats_collector.h"

#include <memory>

#include "base/logging.h"
#include "base/macros.h"
#include "base/test/histogram_tester.h"
#include "chrome/browser/resource_coordinator/tab_manager_web_contents_data.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::WebContents;

namespace resource_coordinator {

class TabManagerStatsCollectorTest : public ChromeRenderViewHostTestHarness {
 public:
  WebContents* CreateWebContents() {
    return content::TestWebContents::Create(profile(), nullptr);
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

}  // namespace resource_coordinator
