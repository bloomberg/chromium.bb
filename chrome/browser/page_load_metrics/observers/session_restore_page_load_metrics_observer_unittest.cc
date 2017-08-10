// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/session_restore_page_load_metrics_observer.h"

#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/test/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_tester.h"
#include "chrome/browser/page_load_metrics/page_load_tracker.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/tab_loader.h"
#include "chrome/common/page_load_metrics/test/page_load_metrics_test_util.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_url_handler.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

using RestoredTab = SessionRestoreDelegate::RestoredTab;
using WebContents = content::WebContents;

class SessionRestorePageLoadMetricsObserverTest
    : public ChromeRenderViewHostTestHarness {
 public:
  static void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) {
    tracker->AddObserver(
        base::MakeUnique<SessionRestorePageLoadMetricsObserver>());
  }

 protected:
  SessionRestorePageLoadMetricsObserverTest() {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    // Add a default web contents.
    AddForegroundTabWithTester();

    // Create the tab manager to register its SessionRestoreObserver before
    // session restore starts.
    g_browser_process->GetTabManager();

    PopulateFirstPaintTimings();
  }

  void TearDown() override {
    // Must be delete tabs before calling TearDown() which cleans up all the
    // testing environment.
    tabs_.clear();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  // Populate first paint and first [contentful,meaningful] paint timings.
  void PopulateFirstPaintTimings() {
    page_load_metrics::InitPageLoadTimingForTest(&timing_);
    timing_.navigation_start = base::Time::FromDoubleT(1);
    // Should be large enough (e.g., >20 ms) for some tests to be able to hide
    // foreground tabs before the first pains.
    timing_.paint_timing->first_meaningful_paint =
        base::TimeDelta::FromSeconds(1);
    PopulateRequiredTimingFields(&timing_);
  }

  WebContents* AddForegroundTabWithTester() {
    tabs_.emplace_back(CreateTestWebContents());
    WebContents* contents = tabs_.back().get();
    auto tester =
        base::MakeUnique<page_load_metrics::PageLoadMetricsObserverTester>(
            contents,
            base::BindRepeating(
                &SessionRestorePageLoadMetricsObserverTest::RegisterObservers));
    testers_[contents] = std::move(tester);
    restored_tabs_.emplace_back(contents, false /* is_active */,
                                false /* is_app */, false /* is_pinned */);
    NavigateAndCommit(contents, GetTestURL());
    contents->WasShown();
    StopLoading(contents);
    return contents;
  }

  // Return the default tab.
  WebContents* web_contents() { return tabs_.front().get(); }

  std::vector<std::unique_ptr<WebContents>>& tabs() { return tabs_; }

  void ExpectFirstPaintMetricsTotalCount(int expected_total_count) const {
    histogram_tester_.ExpectTotalCount(
        internal::kHistogramSessionRestoreForegroundTabFirstPaint,
        expected_total_count);
    histogram_tester_.ExpectTotalCount(
        internal::kHistogramSessionRestoreForegroundTabFirstContentfulPaint,
        expected_total_count);
    histogram_tester_.ExpectTotalCount(
        internal::kHistogramSessionRestoreForegroundTabFirstMeaningfulPaint,
        expected_total_count);
  }

  void RestoreTabs() {
    TabLoader::RestoreTabs(restored_tabs_, base::TimeTicks());
  }

  void SimulateTimingUpdateForTab(WebContents* contents) {
    ASSERT_TRUE(base::ContainsKey(testers_, contents));
    testers_[contents]->SimulateTimingUpdate(timing_);
  }

  void StopLoading(WebContents* contents) const {
    contents->Stop();
    content::WebContentsTester::For(contents)->TestSetIsLoading(false);
  }

  void NavigateAndCommit(WebContents* contents, const GURL& url) const {
    content::NavigationSimulator::NavigateAndCommitFromDocument(
        GetTestURL(), contents->GetMainFrame());
  }

  GURL GetTestURL() const { return GURL("https://google.com"); }

  const page_load_metrics::mojom::PageLoadTiming& timing() const {
    return timing_;
  }

  std::vector<RestoredTab>& restored_tabs() { return restored_tabs_; }

 private:
  base::HistogramTester histogram_tester_;

  page_load_metrics::mojom::PageLoadTiming timing_;
  std::vector<RestoredTab> restored_tabs_;
  std::vector<std::unique_ptr<WebContents>> tabs_;
  std::unordered_map<
      WebContents*,
      std::unique_ptr<page_load_metrics::PageLoadMetricsObserverTester>>
      testers_;

  DISALLOW_COPY_AND_ASSIGN(SessionRestorePageLoadMetricsObserverTest);
};

TEST_F(SessionRestorePageLoadMetricsObserverTest, NoMetrics) {
  ExpectFirstPaintMetricsTotalCount(0);
}

TEST_F(SessionRestorePageLoadMetricsObserverTest,
       FirstPaintsOutOfSessionRestore) {
  NavigateAndCommit(web_contents(), GetTestURL());
  ASSERT_NO_FATAL_FAILURE(SimulateTimingUpdateForTab(web_contents()));
  ExpectFirstPaintMetricsTotalCount(0);
}

TEST_F(SessionRestorePageLoadMetricsObserverTest, RestoreSingleForegroundTab) {
  // Restore one tab which finishes loading in foreground.
  SessionRestore::OnWillRestoreTab(web_contents());
  RestoreTabs();
  NavigateAndCommit(web_contents(), GetTestURL());
  ASSERT_NO_FATAL_FAILURE(SimulateTimingUpdateForTab(web_contents()));
  ExpectFirstPaintMetricsTotalCount(1);
}

TEST_F(SessionRestorePageLoadMetricsObserverTest,
       RestoreMultipleForegroundTabs) {
  WebContents* second_contents = AddForegroundTabWithTester();
  // Restore each tab separately.
  std::vector<std::vector<RestoredTab>> restored_tabs_list{
      std::vector<RestoredTab>{
          RestoredTab(web_contents(), false, false, false)},
      std::vector<RestoredTab>{
          RestoredTab(second_contents, false, false, false)}};

  for (size_t i = 0; i < tabs().size(); ++i) {
    WebContents* contents = tabs()[i].get();
    SessionRestore::OnWillRestoreTab(contents);
    TabLoader::RestoreTabs(restored_tabs_list[i], base::TimeTicks());
    NavigateAndCommit(contents, GetTestURL());
    ASSERT_NO_FATAL_FAILURE(SimulateTimingUpdateForTab(contents));
    ExpectFirstPaintMetricsTotalCount(i + 1);
  }
}

TEST_F(SessionRestorePageLoadMetricsObserverTest, RestoreBackgroundTab) {
  // Set the tab to background before the PageLoadMetricsObserver was created.
  web_contents()->WasHidden();

  // Load the restored tab in background.
  SessionRestore::OnWillRestoreTab(web_contents());
  RestoreTabs();
  NavigateAndCommit(web_contents(), GetTestURL());
  ASSERT_NO_FATAL_FAILURE(SimulateTimingUpdateForTab(web_contents()));

  // No paint timings recorded for tabs restored in background.
  ExpectFirstPaintMetricsTotalCount(0);
}

TEST_F(SessionRestorePageLoadMetricsObserverTest, HideTabBeforeFirstPaints) {
  // Start loading the tab.
  SessionRestore::OnWillRestoreTab(web_contents());
  RestoreTabs();
  NavigateAndCommit(web_contents(), GetTestURL());

  // Hide the tab before any paints.
  web_contents()->WasHidden();
  ASSERT_NO_FATAL_FAILURE(SimulateTimingUpdateForTab(web_contents()));

  // No paint timings recorded because tab was hidden before the first paints.
  ExpectFirstPaintMetricsTotalCount(0);
}

TEST_F(SessionRestorePageLoadMetricsObserverTest,
       SwitchInitialRestoredForegroundTab) {
  // Create 2 tabs: tab 0 is foreground, tab 1 is background.
  AddForegroundTabWithTester();
  restored_tabs()[0].contents()->WasShown();
  restored_tabs()[1].contents()->WasHidden();

  // Restore both tabs.
  for (size_t i = 0; i < tabs().size(); ++i)
    SessionRestore::OnWillRestoreTab(tabs()[i].get());
  TabLoader::RestoreTabs(restored_tabs(), base::TimeTicks());

  for (size_t i = 0; i < tabs().size(); ++i)
    NavigateAndCommit(tabs()[i].get(), GetTestURL());

  // Switch to tab 1 before any paint events occur.
  restored_tabs()[0].contents()->WasHidden();
  restored_tabs()[1].contents()->WasShown();
  ASSERT_NO_FATAL_FAILURE(SimulateTimingUpdateForTab(web_contents()));

  // No paint timings recorded because the initial foreground tab was hidden.
  ExpectFirstPaintMetricsTotalCount(0);
}

TEST_F(SessionRestorePageLoadMetricsObserverTest, MultipleSessionRestores) {
  size_t number_of_session_restores = 3;
  for (size_t i = 1; i <= number_of_session_restores; ++i) {
    // Restore session.
    SessionRestore::OnWillRestoreTab(web_contents());
    RestoreTabs();
    NavigateAndCommit(web_contents(), GetTestURL());
    ASSERT_NO_FATAL_FAILURE(SimulateTimingUpdateForTab(web_contents()));

    // Number of paint timings should match the number of session restores.
    ExpectFirstPaintMetricsTotalCount(i);

    // Clear committed URL for the next restore starts from an empty URL.
    NavigateAndCommit(web_contents(), GURL());
    StopLoading(web_contents());
  }
}
