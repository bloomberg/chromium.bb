// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/aborts_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"

class AbortsPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(make_scoped_ptr(new AbortsPageLoadMetricsObserver()));
  }
};

TEST_F(AbortsPageLoadMetricsObserverTest, AbortStopClose) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  NavigateAndCommit(GURL("https://www.google.com"));
  SimulateTimingUpdate(timing);
  // Simulate the user pressing the stop button.
  web_contents()->Stop();
  NavigateAndCommit(GURL("https://www.example.com"));
  SimulateTimingUpdate(timing);
  // Simulate closing the tab.
  DeleteContents();
  histogram_tester().ExpectTotalCount(internal::kHistogramAbortStopBeforePaint,
                                      1);
  histogram_tester().ExpectTotalCount(internal::kHistogramAbortCloseBeforePaint,
                                      1);
}

TEST_F(AbortsPageLoadMetricsObserverTest, AbortNewNavigation) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  NavigateAndCommit(GURL("https://www.google.com"));
  SimulateTimingUpdate(timing);
  // Simulate the user performaning another navigation before first paint.
  NavigateAndCommit(GURL("https://www.example.com"));
  histogram_tester().ExpectTotalCount(
      internal::kHistogramAbortNewNavigationBeforePaint, 1);
}

TEST_F(AbortsPageLoadMetricsObserverTest, NoAbortNewNavigationFromAboutURL) {
  NavigateAndCommit(GURL("about:blank"));
  NavigateAndCommit(GURL("https://www.example.com"));
  histogram_tester().ExpectTotalCount(
      internal::kHistogramAbortNewNavigationBeforePaint, 0);
}

TEST_F(AbortsPageLoadMetricsObserverTest, NoAbortNewNavigationAfterPaint) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_paint = base::TimeDelta::FromMicroseconds(1);
  NavigateAndCommit(GURL("https://www.google.com"));
  SimulateTimingUpdate(timing);
  NavigateAndCommit(GURL("https://www.example.com"));
  histogram_tester().ExpectTotalCount(
      internal::kHistogramAbortNewNavigationBeforePaint, 0);
}
