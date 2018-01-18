// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/use_counter_page_load_metrics_observer.h"

#include <memory>
#include <vector>
#include "base/macros.h"
#include "base/metrics/histogram_base.h"
#include "base/test/histogram_tester.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "chrome/browser/page_load_metrics/page_load_tracker.h"
#include "third_party/WebKit/public/platform/web_feature.mojom.h"
#include "url/gurl.h"

namespace {

const char kTestUrl[] = "https://www.google.com";
using WebFeature = blink::mojom::WebFeature;

}  // namespace

class UseCounterPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 public:
  UseCounterPageLoadMetricsObserverTest() {}

  void HistogramBasicTest(
      const page_load_metrics::mojom::PageLoadFeatures& first_features,
      const page_load_metrics::mojom::PageLoadFeatures& second_features =
          page_load_metrics::mojom::PageLoadFeatures()) {
    NavigateAndCommit(GURL(kTestUrl));
    SimulateFeaturesUpdate(first_features);
    // Verify that kPageVisits is observed on commit.
    histogram_tester().ExpectBucketCount(
        internal::kFeaturesHistogramName,
        static_cast<base::Histogram::Sample>(WebFeature::kPageVisits), 1);
    for (auto feature : first_features.features) {
      histogram_tester().ExpectBucketCount(
          internal::kFeaturesHistogramName,
          static_cast<base::Histogram::Sample>(feature), 1);
    }
    SimulateFeaturesUpdate(second_features);
    for (auto feature : first_features.features) {
      histogram_tester().ExpectBucketCount(
          internal::kFeaturesHistogramName,
          static_cast<base::Histogram::Sample>(feature), 1);
    }
    for (auto feature : second_features.features) {
      histogram_tester().ExpectBucketCount(
          internal::kFeaturesHistogramName,
          static_cast<base::Histogram::Sample>(feature), 1);
    }
  }

 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(std::make_unique<UseCounterPageLoadMetricsObserver>());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(UseCounterPageLoadMetricsObserverTest);
};

TEST_F(UseCounterPageLoadMetricsObserverTest, CountOneFeature) {
  std::vector<WebFeature> features({WebFeature::kFetch});
  page_load_metrics::mojom::PageLoadFeatures page_load_features;
  page_load_features.features = features;
  HistogramBasicTest(page_load_features);
}

TEST_F(UseCounterPageLoadMetricsObserverTest, CountFeatures) {
  std::vector<WebFeature> features_0(
      {WebFeature::kFetch, WebFeature::kFetchBodyStream});
  std::vector<WebFeature> features_1({WebFeature::kWindowFind});
  page_load_metrics::mojom::PageLoadFeatures page_load_features_0;
  page_load_metrics::mojom::PageLoadFeatures page_load_features_1;
  page_load_features_0.features = features_0;
  page_load_features_1.features = features_1;
  HistogramBasicTest(page_load_features_0, page_load_features_1);
}

TEST_F(UseCounterPageLoadMetricsObserverTest, CountDuplicatedFeatures) {
  std::vector<WebFeature> features_0(
      {WebFeature::kFetch, WebFeature::kFetch, WebFeature::kFetchBodyStream});
  std::vector<WebFeature> features_1(
      {WebFeature::kFetch, WebFeature::kWindowFind});
  page_load_metrics::mojom::PageLoadFeatures page_load_features_0;
  page_load_metrics::mojom::PageLoadFeatures page_load_features_1;
  page_load_features_0.features = features_0;
  page_load_features_1.features = features_1;
  HistogramBasicTest(page_load_features_0, page_load_features_1);
}

TEST_F(UseCounterPageLoadMetricsObserverTest, RecordUkmUsage) {
  std::vector<WebFeature> features_0(
      {WebFeature::kFetch, WebFeature::kNavigatorVibrate});
  std::vector<WebFeature> features_1(
      {WebFeature::kTouchEventPreventedNoTouchAction});
  page_load_metrics::mojom::PageLoadFeatures page_load_features_0;
  page_load_metrics::mojom::PageLoadFeatures page_load_features_1;
  page_load_features_0.features = features_0;
  page_load_features_1.features = features_1;
  HistogramBasicTest(page_load_features_0, page_load_features_1);

  std::vector<const ukm::mojom::UkmEntry*> entries =
      test_ukm_recorder().GetEntriesByName(internal::kUkmUseCounterEventName);
  EXPECT_EQ(2ul, entries.size());
  test_ukm_recorder().ExpectEntrySourceHasUrl(entries[0], GURL(kTestUrl));
  test_ukm_recorder().ExpectEntryMetric(
      entries[0], internal::kUkmUseCounterFeature,
      static_cast<int64_t>(WebFeature::kNavigatorVibrate));
  test_ukm_recorder().ExpectEntrySourceHasUrl(entries[1], GURL(kTestUrl));
  test_ukm_recorder().ExpectEntryMetric(
      entries[1], internal::kUkmUseCounterFeature,
      static_cast<int64_t>(WebFeature::kTouchEventPreventedNoTouchAction));
}
