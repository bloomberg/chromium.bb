// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement_tracker/internal/never_condition_validator.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/test/scoped_feature_list.h"
#include "components/feature_engagement_tracker/internal/model.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feature_engagement_tracker {

namespace {

const base::Feature kTestFeatureFoo{"test_foo",
                                    base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kTestFeatureBar{"test_bar",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// A Model that is always postive to show in-product help.
class TestModel : public Model {
 public:
  TestModel() {
    feature_config_.valid = true;
    feature_config_.feature_used_event = "foobar";
  }

  void Initialize(const OnModelInitializationFinished& callback) override {}

  bool IsReady() const override { return true; }

  const FeatureConfig& GetFeatureConfig(
      const base::Feature& feature) const override {
    return feature_config_;
  }

  void SetIsCurrentlyShowing(bool is_showing) override {}

  bool IsCurrentlyShowing() const override { return false; }

 private:
  FeatureConfig feature_config_;

  DISALLOW_COPY_AND_ASSIGN(TestModel);
};

class NeverConditionValidatorTest : public ::testing::Test {
 public:
  NeverConditionValidatorTest() = default;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  TestModel model_;
  NeverConditionValidator validator_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NeverConditionValidatorTest);
};

}  // namespace

TEST_F(NeverConditionValidatorTest, ShouldNeverMeetConditions) {
  scoped_feature_list_.InitWithFeatures({kTestFeatureFoo, kTestFeatureBar}, {});
  EXPECT_FALSE(validator_.MeetsConditions(kTestFeatureFoo, model_));
  EXPECT_FALSE(validator_.MeetsConditions(kTestFeatureBar, model_));
}

}  // namespace feature_engagement_tracker
