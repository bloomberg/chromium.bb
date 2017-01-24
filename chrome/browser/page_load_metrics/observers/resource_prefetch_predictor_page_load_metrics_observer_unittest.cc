// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/resource_prefetch_predictor_page_load_metrics_observer.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "chrome/browser/predictors/resource_prefetch_common.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"

using predictors::ResourcePrefetchPredictor;

class MockResourcePrefetchPredictor : public ResourcePrefetchPredictor {
 public:
  MockResourcePrefetchPredictor(
      const predictors::ResourcePrefetchPredictorConfig& config,
      Profile* profile)
      : ResourcePrefetchPredictor(config, profile) {}

  MOCK_METHOD1(IsUrlPrefetchable, bool(const GURL& main_frame_url));

  ~MockResourcePrefetchPredictor() override {}
};

class ResourcePrefetchPredictorPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 protected:
  void SetUp() override {
    page_load_metrics::PageLoadMetricsObserverTestHarness::SetUp();
    predictors::ResourcePrefetchPredictorConfig config;
    config.mode = predictors::ResourcePrefetchPredictorConfig::LEARNING;
    predictor_ =
        base::MakeUnique<testing::StrictMock<MockResourcePrefetchPredictor>>(
            config, profile());
    timing_.navigation_start = base::Time::FromDoubleT(1);
    timing_.first_paint = base::TimeDelta::FromSeconds(2);
    timing_.first_contentful_paint = base::TimeDelta::FromSeconds(3);
    timing_.first_meaningful_paint = base::TimeDelta::FromSeconds(4);
    PopulateRequiredTimingFields(&timing_);
  }

  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(
        base::MakeUnique<ResourcePrefetchPredictorPageLoadMetricsObserver>(
            predictor_.get()));
  }

  std::unique_ptr<testing::StrictMock<MockResourcePrefetchPredictor>>
      predictor_;
  page_load_metrics::PageLoadTiming timing_;
};

TEST_F(ResourcePrefetchPredictorPageLoadMetricsObserverTest,
       PrefetchableIsRecorded) {
  const GURL main_frame_url("https://www.google.com");
  EXPECT_CALL(*predictor_.get(), IsUrlPrefetchable(main_frame_url))
      .WillOnce(testing::Return(true));

  NavigateAndCommit(main_frame_url);
  SimulateTimingUpdate(timing_);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramResourcePrefetchPredictorFirstContentfulPaint, 1);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramResourcePrefetchPredictorFirstMeaningfulPaint, 1);
}

TEST_F(ResourcePrefetchPredictorPageLoadMetricsObserverTest,
       NotPrefetchableIsNotRecorded) {
  const GURL main_frame_url("https://www.google.com");
  EXPECT_CALL(*predictor_.get(), IsUrlPrefetchable(main_frame_url))
      .WillOnce(testing::Return(false));

  NavigateAndCommit(main_frame_url);
  SimulateTimingUpdate(timing_);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramResourcePrefetchPredictorFirstContentfulPaint, 0);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramResourcePrefetchPredictorFirstMeaningfulPaint, 0);
}
