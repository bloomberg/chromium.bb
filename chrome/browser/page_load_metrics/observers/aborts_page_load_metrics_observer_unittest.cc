// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/aborts_page_load_metrics_observer.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "chrome/browser/page_load_metrics/page_load_tracker.h"
#include "chrome/common/page_load_metrics/test/page_load_metrics_test_util.h"
#include "content/public/browser/web_contents.h"

class AbortsPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(base::MakeUnique<AbortsPageLoadMetricsObserver>());
  }

  void SimulateTimingWithoutPaint() {
    page_load_metrics::mojom::PageLoadTiming timing;
    page_load_metrics::InitPageLoadTimingForTest(&timing);
    timing.navigation_start = base::Time::FromDoubleT(1);
    SimulateTimingUpdate(timing);
  }

  int CountTotalAbortMetricsRecorded() {
    base::HistogramTester::CountsMap counts_map =
        histogram_tester().GetTotalCountsForPrefix(
            "PageLoad.Experimental.AbortTiming.");
    int count = 0;
    for (const auto& entry : counts_map)
      count += entry.second;
    return count;
  }
};

TEST_F(AbortsPageLoadMetricsObserverTest, NewNavigationBeforeCommit) {
  StartNavigation(GURL("https://www.google.com"));
  // Simulate the user performing another navigation before commit.
  NavigateAndCommit(GURL("https://www.example.com"));
  histogram_tester().ExpectTotalCount(
      internal::kHistogramAbortNewNavigationBeforeCommit, 1);
}

TEST_F(AbortsPageLoadMetricsObserverTest, ReloadBeforeCommit) {
  StartNavigation(GURL("https://www.google.com"));
  // Simulate the user performing another navigation before commit.
  NavigateWithPageTransitionAndCommit(GURL("https://www.example.com"),
                                      ui::PAGE_TRANSITION_RELOAD);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramAbortReloadBeforeCommit, 1);
}

TEST_F(AbortsPageLoadMetricsObserverTest, ForwardBackBeforeCommit) {
  StartNavigation(GURL("https://www.google.com"));
  // Simulate the user performing another navigation before commit.
  NavigateWithPageTransitionAndCommit(GURL("https://www.example.com"),
                                      ui::PAGE_TRANSITION_FORWARD_BACK);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramAbortForwardBackBeforeCommit, 1);
}

TEST_F(AbortsPageLoadMetricsObserverTest, BackgroundBeforeCommit) {
  StartNavigation(GURL("https://www.google.com"));
  // Simulate the tab being backgrounded.
  web_contents()->WasHidden();

  NavigateAndCommit(GURL("about:blank"));
  histogram_tester().ExpectTotalCount(
      internal::kHistogramAbortBackgroundBeforeCommit, 1);
  EXPECT_EQ(1, CountTotalAbortMetricsRecorded());
}

TEST_F(AbortsPageLoadMetricsObserverTest,
       NewProvisionalNavigationBeforeCommit) {
  StartNavigation(GURL("https://www.google.com"));
  StartNavigation(GURL("https://www.example.com"));
  histogram_tester().ExpectTotalCount(
      internal::kHistogramAbortNewNavigationBeforeCommit, 1);
}

TEST_F(AbortsPageLoadMetricsObserverTest,
       NewNavigationBeforeCommitNonTrackedPageLoad) {
  StartNavigation(GURL("https://www.google.com"));
  // Simulate the user performing another navigation before commit. Navigate to
  // an untracked URL, to verify that we still log abort metrics even if the new
  // navigation isn't tracked.
  NavigateAndCommit(GURL("about:blank"));
  histogram_tester().ExpectTotalCount(
      internal::kHistogramAbortNewNavigationBeforeCommit, 1);
}

TEST_F(AbortsPageLoadMetricsObserverTest, NewNavigationBeforePaint) {
  NavigateAndCommit(GURL("https://www.google.com"));
  SimulateTimingWithoutPaint();
  // Simulate the user performing another navigation before paint.
  NavigateAndCommit(GURL("https://www.example.com"));
  histogram_tester().ExpectTotalCount(
      internal::kHistogramAbortNewNavigationBeforePaint, 1);
}

TEST_F(AbortsPageLoadMetricsObserverTest, ReloadBeforePaint) {
  NavigateAndCommit(GURL("https://www.example.com"));
  SimulateTimingWithoutPaint();
  // Simulate the user performing a reload navigation before paint.
  NavigateWithPageTransitionAndCommit(GURL("https://www.google.com"),
                                      ui::PAGE_TRANSITION_RELOAD);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramAbortReloadBeforePaint, 1);
}

TEST_F(AbortsPageLoadMetricsObserverTest, ForwardBackBeforePaint) {
  NavigateAndCommit(GURL("https://www.example.com"));
  SimulateTimingWithoutPaint();
  // Simulate the user performing a forward/back navigation before paint.
  NavigateWithPageTransitionAndCommit(
      GURL("https://www.google.com"),
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FORWARD_BACK));
  histogram_tester().ExpectTotalCount(
      internal::kHistogramAbortForwardBackBeforePaint, 1);
}

TEST_F(AbortsPageLoadMetricsObserverTest, BackgroundBeforePaint) {
  NavigateAndCommit(GURL("https://www.example.com"));
  SimulateTimingWithoutPaint();
  // Simulate the tab being backgrounded.
  web_contents()->WasHidden();
  NavigateAndCommit(GURL("https://www.google.com"));
  histogram_tester().ExpectTotalCount(
      internal::kHistogramAbortBackgroundBeforePaint, 1);
  EXPECT_EQ(1, CountTotalAbortMetricsRecorded());
}

TEST_F(AbortsPageLoadMetricsObserverTest, StopBeforeCommit) {
  StartNavigation(GURL("https://www.google.com"));
  // Simulate the user pressing the stop button.
  web_contents()->Stop();
  // Now close the tab. This will trigger logging for the prior navigation which
  // was stopped above.
  DeleteContents();
  histogram_tester().ExpectTotalCount(internal::kHistogramAbortStopBeforeCommit,
                                      1);
  EXPECT_EQ(1, CountTotalAbortMetricsRecorded());
}

TEST_F(AbortsPageLoadMetricsObserverTest, StopBeforePaint) {
  NavigateAndCommit(GURL("https://www.google.com"));
  SimulateTimingWithoutPaint();
  // Simulate the user pressing the stop button.
  web_contents()->Stop();
  // Now close the tab. This will trigger logging for the prior navigation which
  // was stopped above.
  DeleteContents();
  histogram_tester().ExpectTotalCount(internal::kHistogramAbortStopBeforePaint,
                                      1);
  EXPECT_EQ(1, CountTotalAbortMetricsRecorded());
}

TEST_F(AbortsPageLoadMetricsObserverTest, StopBeforeCommitAndBeforePaint) {
  // Commit the first navigation.
  NavigateAndCommit(GURL("https://www.google.com"));
  SimulateTimingWithoutPaint();
  // Now start a second navigation, but don't commit it.
  StartNavigation(GURL("https://www.google.com"));
  // Simulate the user pressing the stop button. This should cause us to record
  // two abort stop histograms, one before commit and the other before paint.
  web_contents()->Stop();
  // Simulate closing the tab.
  DeleteContents();
  histogram_tester().ExpectTotalCount(internal::kHistogramAbortStopBeforeCommit,
                                      1);
  histogram_tester().ExpectTotalCount(internal::kHistogramAbortStopBeforePaint,
                                      1);
  EXPECT_EQ(2, CountTotalAbortMetricsRecorded());
}

TEST_F(AbortsPageLoadMetricsObserverTest, CloseBeforeCommit) {
  StartNavigation(GURL("https://www.google.com"));
  // Simulate closing the tab.
  DeleteContents();
  histogram_tester().ExpectTotalCount(
      internal::kHistogramAbortCloseBeforeCommit, 1);
  EXPECT_EQ(1, CountTotalAbortMetricsRecorded());
}

TEST_F(AbortsPageLoadMetricsObserverTest, CloseBeforePaint) {
  NavigateAndCommit(GURL("https://www.google.com"));
  SimulateTimingWithoutPaint();
  // Simulate closing the tab.
  DeleteContents();
  histogram_tester().ExpectTotalCount(internal::kHistogramAbortCloseBeforePaint,
                                      1);
  EXPECT_EQ(1, CountTotalAbortMetricsRecorded());
}

TEST_F(AbortsPageLoadMetricsObserverTest,
       AbortCloseBeforeCommitAndBeforePaint) {
  // Commit the first navigation.
  NavigateAndCommit(GURL("https://www.google.com"));
  SimulateTimingWithoutPaint();
  // Now start a second navigation, but don't commit it.
  StartNavigation(GURL("https://www.google.com"));
  // Simulate closing the tab.
  DeleteContents();
  histogram_tester().ExpectTotalCount(
      internal::kHistogramAbortCloseBeforeCommit, 1);
  histogram_tester().ExpectTotalCount(internal::kHistogramAbortCloseBeforePaint,
                                      1);
  EXPECT_EQ(2, CountTotalAbortMetricsRecorded());
}

TEST_F(AbortsPageLoadMetricsObserverTest,
       AbortStopBeforeCommitAndCloseBeforePaint) {
  StartNavigation(GURL("https://www.google.com"));
  // Simulate the user pressing the stop button.
  web_contents()->Stop();
  NavigateAndCommit(GURL("https://www.example.com"));
  SimulateTimingWithoutPaint();
  // Simulate closing the tab.
  DeleteContents();
  histogram_tester().ExpectTotalCount(internal::kHistogramAbortStopBeforeCommit,
                                      1);
  histogram_tester().ExpectTotalCount(internal::kHistogramAbortCloseBeforePaint,
                                      1);
  EXPECT_EQ(2, CountTotalAbortMetricsRecorded());
}

TEST_F(AbortsPageLoadMetricsObserverTest, NoAbortNewNavigationFromAboutURL) {
  NavigateAndCommit(GURL("about:blank"));
  NavigateAndCommit(GURL("https://www.example.com"));
  histogram_tester().ExpectTotalCount(
      internal::kHistogramAbortNewNavigationBeforePaint, 0);
  EXPECT_EQ(0, CountTotalAbortMetricsRecorded());
}

TEST_F(AbortsPageLoadMetricsObserverTest,
       NoAbortNewNavigationFromURLWithoutTiming) {
  NavigateAndCommit(GURL("https://www.google.com"));
  // Simulate the user performing another navigation before paint.
  NavigateAndCommit(GURL("https://www.example.com"));
  // Since the navigation to google.com had no timing information associated
  // with it, no abort is logged.
  histogram_tester().ExpectTotalCount(
      internal::kHistogramAbortNewNavigationBeforePaint, 0);
  EXPECT_EQ(0, CountTotalAbortMetricsRecorded());
}

TEST_F(AbortsPageLoadMetricsObserverTest, NoAbortNewNavigationAfterPaint) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.paint_timing->first_paint = base::TimeDelta::FromMicroseconds(1);
  PopulateRequiredTimingFields(&timing);
  NavigateAndCommit(GURL("https://www.google.com"));
  SimulateTimingUpdate(timing);

  // The test cannot assume that abort time will be > first_paint
  // (1 micro-sec). If the system clock is low resolution, PageLoadTracker's
  // abort time may be <= first_paint. In that case the histogram will be
  // logged. Thus both 0 and 1 counts of histograms are considered good.

  NavigateAndCommit(GURL("https://www.example.com"));

  base::HistogramTester::CountsMap counts_map =
      histogram_tester().GetTotalCountsForPrefix(
          internal::kHistogramAbortNewNavigationBeforePaint);

  EXPECT_TRUE(counts_map.empty() ||
              (counts_map.size() == 1 && counts_map.begin()->second == 1));
}
