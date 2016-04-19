// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/aborts_page_load_metrics_observer.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"

class AbortsPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(base::WrapUnique(new AbortsPageLoadMetricsObserver()));
  }

  void SimulateTimingWithoutPaint() {
    page_load_metrics::PageLoadTiming timing;
    timing.navigation_start = base::Time::FromDoubleT(1);
    SimulateTimingUpdate(timing);
  }
};

TEST_F(AbortsPageLoadMetricsObserverTest, UnknownNavigationBeforeCommit) {
  StartNavigation(GURL("https://www.google.com"));
  // Simulate the user performing another navigation before commit.
  NavigateAndCommit(GURL("https://www.example.com"));
  histogram_tester().ExpectTotalCount(
      internal::kHistogramAbortUnknownNavigationBeforeCommit, 1);
}

TEST_F(AbortsPageLoadMetricsObserverTest, NewNavigationBeforePaint) {
  NavigateAndCommit(GURL("https://www.google.com"));
  SimulateTimingWithoutPaint();
  // Simulate the user performing another navigation before paint.
  NavigateAndCommit(GURL("https://www.example.com"));
  histogram_tester().ExpectTotalCount(
      internal::kHistogramAbortNewNavigationBeforePaint, 1);
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
}

TEST_F(AbortsPageLoadMetricsObserverTest, CloseBeforeCommit) {
  StartNavigation(GURL("https://www.google.com"));
  // Simulate closing the tab.
  DeleteContents();
  histogram_tester().ExpectTotalCount(
      internal::kHistogramAbortCloseBeforeCommit, 1);
}

TEST_F(AbortsPageLoadMetricsObserverTest, CloseBeforePaint) {
  NavigateAndCommit(GURL("https://www.google.com"));
  SimulateTimingWithoutPaint();
  // Simulate closing the tab.
  DeleteContents();
  histogram_tester().ExpectTotalCount(internal::kHistogramAbortCloseBeforePaint,
                                      1);
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
}

// TODO(bmcquade): add tests for reload, back/forward, and other aborts.

TEST_F(AbortsPageLoadMetricsObserverTest, NoAbortNewNavigationFromAboutURL) {
  NavigateAndCommit(GURL("about:blank"));
  NavigateAndCommit(GURL("https://www.example.com"));
  histogram_tester().ExpectTotalCount(
      internal::kHistogramAbortNewNavigationBeforePaint, 0);
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
}

TEST_F(AbortsPageLoadMetricsObserverTest, NoAbortNewNavigationAfterPaint) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_paint = base::TimeDelta::FromMicroseconds(1);
  PopulateRequiredTimingFields(&timing);
  NavigateAndCommit(GURL("https://www.google.com"));
  SimulateTimingUpdate(timing);
  NavigateAndCommit(GURL("https://www.example.com"));
  histogram_tester().ExpectTotalCount(
      internal::kHistogramAbortNewNavigationBeforePaint, 0);
}
