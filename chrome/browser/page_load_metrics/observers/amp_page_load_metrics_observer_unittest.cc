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
    timing_.parse_timing.parse_start = base::TimeDelta::FromSeconds(3);
    timing_.paint_timing.first_contentful_paint =
        base::TimeDelta::FromSeconds(4);
    timing_.paint_timing.first_image_paint = base::TimeDelta::FromSeconds(5);
    timing_.paint_timing.first_text_paint = base::TimeDelta::FromSeconds(6);
    timing_.document_timing.load_event_start = base::TimeDelta::FromSeconds(7);
    PopulateRequiredTimingFields(&timing_);
  }

  void RunTest(const GURL& url) {
    NavigateAndCommit(url);
    SimulateTimingUpdate(timing_);

    // Navigate again to force OnComplete, which happens when a new navigation
    // occurs.
    NavigateAndCommit(GURL("http://otherurl.com"));
  }

  void ValidateHistograms(bool expect_histograms, const char* view_type) {
    ValidateHistogramsFor(
        "PageLoad.Clients.AMP.DocumentTiming."
        "NavigationToDOMContentLoadedEventFired",
        view_type, timing_.document_timing.dom_content_loaded_event_start,
        expect_histograms);
    ValidateHistogramsFor(
        "PageLoad.Clients.AMP.DocumentTiming.NavigationToFirstLayout",
        view_type, timing_.document_timing.first_layout, expect_histograms);
    ValidateHistogramsFor(
        "PageLoad.Clients.AMP.DocumentTiming."
        "NavigationToLoadEventFired",
        view_type, timing_.document_timing.load_event_start, expect_histograms);
    ValidateHistogramsFor(
        "PageLoad.Clients.AMP.PaintTiming."
        "NavigationToFirstContentfulPaint",
        view_type, timing_.paint_timing.first_contentful_paint,
        expect_histograms);
    ValidateHistogramsFor(
        "PageLoad.Clients.AMP.ParseTiming.NavigationToParseStart", view_type,
        timing_.parse_timing.parse_start, expect_histograms);
  }

  void ValidateHistogramsFor(const std::string& histogram,
                             const char* view_type,
                             const base::Optional<base::TimeDelta>& event,
                             bool expect_histograms) {
    const size_t kTypeOffset = strlen("PageLoad.Clients.AMP.");
    std::string view_type_histogram = histogram;
    view_type_histogram.insert(kTypeOffset, view_type);
    histogram_tester().ExpectTotalCount(histogram, expect_histograms ? 1 : 0);
    histogram_tester().ExpectTotalCount(view_type_histogram,
                                        expect_histograms ? 1 : 0);
    if (!expect_histograms)
      return;
    histogram_tester().ExpectUniqueSample(
        histogram,
        static_cast<base::HistogramBase::Sample>(
            event.value().InMilliseconds()),
        1);
    histogram_tester().ExpectUniqueSample(
        view_type_histogram,
        static_cast<base::HistogramBase::Sample>(
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

TEST_F(AMPPageLoadMetricsObserverTest, AMPViewType) {
  using AMPViewType = AMPPageLoadMetricsObserver::AMPViewType;

  struct {
    AMPViewType expected_type;
    const char* url;
  } test_cases[] = {
      {AMPViewType::NONE, "https://google.com/"},
      {AMPViewType::NONE, "https://google.com/amp/foo"},
      {AMPViewType::NONE, "https://google.com/news/amp?foo"},
      {AMPViewType::NONE, "https://example.com/"},
      {AMPViewType::NONE, "https://example.com/amp/foo"},
      {AMPViewType::NONE, "https://example.com/news/amp?foo"},
      {AMPViewType::NONE, "https://www.google.com/"},
      {AMPViewType::NONE, "https://news.google.com/"},
      {AMPViewType::AMP_CACHE, "https://cdn.ampproject.org/foo"},
      {AMPViewType::AMP_CACHE, "https://site.cdn.ampproject.org/foo"},
      {AMPViewType::GOOGLE_SEARCH_AMP_VIEWER, "https://www.google.com/amp/foo"},
      {AMPViewType::GOOGLE_NEWS_AMP_VIEWER,
       "https://news.google.com/news/amp?foo"},
  };
  for (const auto& test : test_cases) {
    EXPECT_EQ(test.expected_type,
              AMPPageLoadMetricsObserver::GetAMPViewType(GURL(test.url)))
        << "For URL: " << test.url;
  }
}

TEST_F(AMPPageLoadMetricsObserverTest, AMPCachePage) {
  ResetTest();
  RunTest(GURL("https://cdn.ampproject.org/page"));
  ValidateHistograms(true, "AmpCache.");
}

TEST_F(AMPPageLoadMetricsObserverTest, GoogleSearchAMPCachePage) {
  ResetTest();
  RunTest(GURL("https://www.google.com/amp/page"));
  ValidateHistograms(true, "GoogleSearch.");
}

TEST_F(AMPPageLoadMetricsObserverTest, GoogleNewsAMPCachePage) {
  ResetTest();
  RunTest(GURL("https://news.google.com/news/amp?page"));
  ValidateHistograms(true, "GoogleNews.");
}

TEST_F(AMPPageLoadMetricsObserverTest, NonAMPPage) {
  ResetTest();
  RunTest(GURL("https://www.google.com/not-amp/page"));
  ValidateHistograms(false, "");
}
