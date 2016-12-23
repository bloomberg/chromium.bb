// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/previews_page_load_metrics_observer.h"

#include <string>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "chrome/common/page_load_metrics/page_load_timing.h"
#include "content/public/browser/web_contents.h"

namespace previews {

namespace {

const char kDefaultTestUrl1[] = "https://google.com";
const char kDefaultTestUrl2[] = "https://example.com";

class TestPreviewsPageLoadMetricsObserver
    : public PreviewsPageLoadMetricsObserver {
 public:
  explicit TestPreviewsPageLoadMetricsObserver(bool offline_preview)
      : offline_preview_(offline_preview) {}
  ~TestPreviewsPageLoadMetricsObserver() override {}

  bool IsOfflinePreview(content::WebContents* web_contents) const override {
    return offline_preview_;
  }

 private:
  bool offline_preview_;
};

}  // namespace

class PreviewsPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 public:
  PreviewsPageLoadMetricsObserverTest() : is_offline_preview_(false) {}

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

  void RunTest(bool is_offline_preview) {
    is_offline_preview_ = is_offline_preview;
    NavigateAndCommit(GURL(kDefaultTestUrl1));
    SimulateTimingUpdate(timing_);

    // Navigate again to force OnComplete, which happens when a new navigation
    // occurs.
    NavigateAndCommit(GURL(kDefaultTestUrl2));
  }

  void ValidateHistograms() {
    ValidateHistogramsFor(
        internal::kHistogramOfflinePreviewsDOMContentLoadedEventFired,
        timing_.dom_content_loaded_event_start);
    ValidateHistogramsFor(internal::kHistogramOfflinePreviewsFirstLayout,
                          timing_.first_layout);
    ValidateHistogramsFor(internal::kHistogramOfflinePreviewsLoadEventFired,
                          timing_.load_event_start);
    ValidateHistogramsFor(
        internal::kHistogramOfflinePreviewsFirstContentfulPaint,
        timing_.first_contentful_paint);
    ValidateHistogramsFor(internal::kHistogramOfflinePreviewsParseStart,
                          timing_.parse_start);
  }

  void ValidateHistogramsFor(const std::string& histogram_,
                             const base::Optional<base::TimeDelta>& event) {
    histogram_tester().ExpectTotalCount(histogram_,
                                        is_offline_preview_ ? 1 : 0);
    if (!is_offline_preview_)
      return;
    histogram_tester().ExpectUniqueSample(
        histogram_, static_cast<base::HistogramBase::Sample>(
                        event.value().InMilliseconds()),
        1);
  }

 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(base::WrapUnique(
        new TestPreviewsPageLoadMetricsObserver(is_offline_preview_)));
  }

 private:
  page_load_metrics::PageLoadTiming timing_;
  bool is_offline_preview_;

  DISALLOW_COPY_AND_ASSIGN(PreviewsPageLoadMetricsObserverTest);
};

TEST_F(PreviewsPageLoadMetricsObserverTest, NoPreview) {
  ResetTest();
  RunTest(false);
  ValidateHistograms();
}

TEST_F(PreviewsPageLoadMetricsObserverTest, OfflinePreviews) {
  ResetTest();
  RunTest(true);
  ValidateHistograms();
}

}  // namespace previews
