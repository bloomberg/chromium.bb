// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/amp_page_load_metrics_observer.h"

#include <string>

#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "chrome/common/page_load_metrics/page_load_timing.h"
#include "url/gurl.h"

class AMPPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 public:
  AMPPageLoadMetricsObserverTest() {}

  void ResetTest() {
    // Reset to the default testing state. Does not reset histogram state.
    timing_.navigation_start = base::Time::FromDoubleT(1);
    timing_.response_start = base::TimeDelta::FromSeconds(2);
    timing_.parse_start = base::TimeDelta::FromSeconds(3);
    timing_.first_contentful_paint = base::TimeDelta::FromSeconds(4);
    timing_.first_image_paint = base::TimeDelta::FromSeconds(5);
    timing_.first_text_paint = base::TimeDelta::FromSeconds(6);
    timing_.load_event_start = base::TimeDelta::FromSeconds(7);
    PopulateRequiredTimingFields(&timing_);
  }

  void RunTest(const GURL& url) {
    NavigateAndCommit(url);
    SimulateTimingUpdate(timing_);

    // Navigate again to force OnComplete, which happens when a new navigation
    // occurs.
    NavigateAndCommit(GURL("http://otherurl.com"));
  }

  void ValidateHistograms(bool expect_histograms) {
    ValidateHistogramsFor(
        "PageLoad.Clients.AMPCache2.DocumentTiming."
        "NavigationToDOMContentLoadedEventFired",
        timing_.dom_content_loaded_event_start, expect_histograms);
    ValidateHistogramsFor(
        "PageLoad.Clients.AMPCache2.DocumentTiming.NavigationToFirstLayout",
        timing_.first_layout, expect_histograms);
    ValidateHistogramsFor(
        "PageLoad.Clients.AMPCache2.DocumentTiming."
        "NavigationToLoadEventFired",
        timing_.load_event_start, expect_histograms);
    ValidateHistogramsFor(
        "PageLoad.Clients.AMPCache2.PaintTiming."
        "NavigationToFirstContentfulPaint",
        timing_.first_contentful_paint, expect_histograms);
    ValidateHistogramsFor(
        "PageLoad.Clients.AMPCache2.ParseTiming.NavigationToParseStart",
        timing_.parse_start, expect_histograms);
  }

  void ValidateHistogramsFor(const std::string& histogram_,
                             const base::Optional<base::TimeDelta>& event,
                             bool expect_histograms) {
    histogram_tester().ExpectTotalCount(histogram_, expect_histograms ? 1 : 0);
    if (!expect_histograms)
      return;
    histogram_tester().ExpectUniqueSample(
        histogram_, static_cast<base::HistogramBase::Sample>(
                        event.value().InMilliseconds()),
        1);
  }

 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(base::WrapUnique(new AMPPageLoadMetricsObserver()));
  }

 private:
  page_load_metrics::PageLoadTiming timing_;

  DISALLOW_COPY_AND_ASSIGN(AMPPageLoadMetricsObserverTest);
};

TEST_F(AMPPageLoadMetricsObserverTest, AMPCachePage) {
  ResetTest();
  RunTest(GURL("https://cdn.ampproject.org/page"));
  ValidateHistograms(true);
}

TEST_F(AMPPageLoadMetricsObserverTest, GoogleAMPCachePage) {
  ResetTest();
  RunTest(GURL("https://www.google.com/amp/page"));
  ValidateHistograms(true);
}

TEST_F(AMPPageLoadMetricsObserverTest, NonAMPPage) {
  ResetTest();
  RunTest(GURL("https://www.google.com/not-amp/page"));
  ValidateHistograms(false);
}
