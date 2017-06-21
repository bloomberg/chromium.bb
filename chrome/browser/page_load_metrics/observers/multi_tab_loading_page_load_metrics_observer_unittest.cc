// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/multi_tab_loading_page_load_metrics_observer.h"

#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"

namespace {

const char kDefaultTestUrl[] = "https://google.com";

class TestMultiTabLoadingPageLoadMetricsObserver
    : public MultiTabLoadingPageLoadMetricsObserver {
 public:
  explicit TestMultiTabLoadingPageLoadMetricsObserver(bool multi_tab_loading)
      : multi_tab_loading_(multi_tab_loading) {}
  ~TestMultiTabLoadingPageLoadMetricsObserver() override {}

 private:
  bool IsAnyTabLoading(content::NavigationHandle* navigation_handle) override {
    return multi_tab_loading_;
  }

  const bool multi_tab_loading_;
};

}  // namespace

class MultiTabLoadingPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 public:
  enum UseCase { SingleTabLoading, MultiTabLoading };
  enum TabState { Foreground, Background };

  void SimulatePageLoad(UseCase use_case, TabState tab_state) {
    use_case_ = use_case;

    page_load_metrics::mojom::PageLoadTiming timing;
    page_load_metrics::InitPageLoadTimingForTest(&timing);
    timing.navigation_start = base::Time::FromDoubleT(1);
    timing.paint_timing->first_contentful_paint =
        base::TimeDelta::FromMilliseconds(300);
    timing.paint_timing->first_meaningful_paint =
        base::TimeDelta::FromMilliseconds(700);
    timing.document_timing->dom_content_loaded_event_start =
        base::TimeDelta::FromMilliseconds(600);
    timing.document_timing->load_event_start =
        base::TimeDelta::FromMilliseconds(1000);
    PopulateRequiredTimingFields(&timing);

    if (tab_state == Background) {
      // Start in background.
      web_contents()->WasHidden();
    }

    NavigateAndCommit(GURL(kDefaultTestUrl));

    if (tab_state == Background) {
      // Foreground the tab.
      web_contents()->WasShown();
    }

    SimulateTimingUpdate(timing);
  }

 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(
        base::MakeUnique<TestMultiTabLoadingPageLoadMetricsObserver>(
            use_case_.value() == MultiTabLoading));
  }

 private:
  base::Optional<UseCase> use_case_;
};

TEST_F(MultiTabLoadingPageLoadMetricsObserverTest, SingleTabLoading) {
  SimulatePageLoad(SingleTabLoading, Foreground);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramMultiTabLoadingFirstContentfulPaint, 0);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramMultiTabLoadingForegroundToFirstContentfulPaint, 0);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramMultiTabLoadingFirstMeaningfulPaint, 0);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramMultiTabLoadingForegroundToFirstMeaningfulPaint, 0);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramMultiTabLoadingDomContentLoaded, 0);
  histogram_tester().ExpectTotalCount(
      internal::kBackgroundHistogramMultiTabLoadingDomContentLoaded, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramMultiTabLoadingLoad,
                                      0);
  histogram_tester().ExpectTotalCount(
      internal::kBackgroundHistogramMultiTabLoadingLoad, 0);
}

TEST_F(MultiTabLoadingPageLoadMetricsObserverTest, MultiTabLoading) {
  SimulatePageLoad(MultiTabLoading, Foreground);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramMultiTabLoadingFirstContentfulPaint, 1);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramMultiTabLoadingForegroundToFirstContentfulPaint, 0);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramMultiTabLoadingFirstMeaningfulPaint, 1);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramMultiTabLoadingForegroundToFirstMeaningfulPaint, 0);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramMultiTabLoadingDomContentLoaded, 1);
  histogram_tester().ExpectTotalCount(
      internal::kBackgroundHistogramMultiTabLoadingDomContentLoaded, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramMultiTabLoadingLoad,
                                      1);
  histogram_tester().ExpectTotalCount(
      internal::kBackgroundHistogramMultiTabLoadingLoad, 0);
}

TEST_F(MultiTabLoadingPageLoadMetricsObserverTest, MultiTabBackground) {
  SimulatePageLoad(MultiTabLoading, Background);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramMultiTabLoadingFirstContentfulPaint, 0);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramMultiTabLoadingForegroundToFirstContentfulPaint, 1);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramMultiTabLoadingFirstMeaningfulPaint, 0);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramMultiTabLoadingForegroundToFirstMeaningfulPaint, 1);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramMultiTabLoadingDomContentLoaded, 0);
  histogram_tester().ExpectTotalCount(
      internal::kBackgroundHistogramMultiTabLoadingDomContentLoaded, 1);
  histogram_tester().ExpectTotalCount(internal::kHistogramMultiTabLoadingLoad,
                                      0);
  histogram_tester().ExpectTotalCount(
      internal::kBackgroundHistogramMultiTabLoadingLoad, 1);
}
