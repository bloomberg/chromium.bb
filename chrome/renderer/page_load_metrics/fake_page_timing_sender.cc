// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/page_load_metrics/fake_page_timing_sender.h"

#include <algorithm>
#include "testing/gtest/include/gtest/gtest.h"

namespace page_load_metrics {

FakePageTimingSender::FakePageTimingSender(PageTimingValidator* validator)
    : validator_(validator) {}

FakePageTimingSender::~FakePageTimingSender() {}

void FakePageTimingSender::SendTiming(
    const mojom::PageLoadTimingPtr& timing,
    const mojom::PageLoadMetadataPtr& metadata,
    mojom::PageLoadFeaturesPtr new_features) {
  validator_->UpdateTiming(timing, metadata, new_features);
}

FakePageTimingSender::PageTimingValidator::PageTimingValidator() {}

FakePageTimingSender::PageTimingValidator::~PageTimingValidator() {
  VerifyExpectedTimings();
}

void FakePageTimingSender::PageTimingValidator::ExpectPageLoadTiming(
    const mojom::PageLoadTiming& timing) {
  VerifyExpectedTimings();
  expected_timings_.push_back(timing.Clone());
}

void FakePageTimingSender::PageTimingValidator::VerifyExpectedTimings() const {
  // Ideally we'd just call ASSERT_EQ(actual_timings_, expected_timings_) here,
  // but this causes the generated gtest code to fail to build on Windows. See
  // the comments in the header file for additional details.
  ASSERT_EQ(actual_timings_.size(), expected_timings_.size());
  for (size_t i = 0; i < actual_timings_.size(); ++i) {
    if (actual_timings_.at(i)->Equals(*expected_timings_.at(i)))
      continue;
    ADD_FAILURE() << "Actual timing != expected timing at index " << i;
  }
}

void FakePageTimingSender::PageTimingValidator::UpdateExpectPageLoadFeatures(
    const blink::mojom::WebFeature feature) {
  expected_features_.insert(feature);
}

void FakePageTimingSender::PageTimingValidator::VerifyExpectedFeatures() const {
  ASSERT_EQ(actual_features_.size(), expected_features_.size());
  std::vector<blink::mojom::WebFeature> diff;
  std::set_difference(actual_features_.begin(), actual_features_.end(),
                      expected_features_.begin(), expected_features_.end(),
                      diff.begin());
  EXPECT_TRUE(diff.empty())
      << "Expected more features than the actual features observed";

  std::set_difference(expected_features_.begin(), expected_features_.end(),
                      actual_features_.begin(), actual_features_.end(),
                      diff.begin());
  EXPECT_TRUE(diff.empty())
      << "More features are actually observed than expected";
}

void FakePageTimingSender::PageTimingValidator::UpdateTiming(
    const mojom::PageLoadTimingPtr& timing,
    const mojom::PageLoadMetadataPtr& metadata,
    const mojom::PageLoadFeaturesPtr& new_features) {
  actual_timings_.push_back(timing.Clone());
  for (const auto feature : new_features->features) {
    EXPECT_EQ(actual_features_.find(feature), actual_features_.end())
        << "Feature " << feature << "has been sent more than once";
    actual_features_.insert(feature);
  }
  VerifyExpectedTimings();
  VerifyExpectedFeatures();
}

}  // namespace page_load_metrics
