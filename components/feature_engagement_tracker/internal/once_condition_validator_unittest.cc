// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement_tracker/internal/once_condition_validator.h"

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/test/scoped_feature_list.h"
#include "components/feature_engagement_tracker/internal/editable_configuration.h"
#include "components/feature_engagement_tracker/internal/model.h"
#include "components/feature_engagement_tracker/internal/proto/event.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feature_engagement_tracker {

namespace {

const base::Feature kTestFeatureFoo{"test_foo",
                                    base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kTestFeatureBar{"test_bar",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// A Model that is easily configurable at runtime.
class TestModel : public Model {
 public:
  TestModel() : ready_(false), is_showing_(false) {}

  void Initialize(const OnModelInitializationFinished& callback) override {}

  bool IsReady() const override { return ready_; }

  void SetIsReady(bool ready) { ready_ = ready; }

  const FeatureConfig& GetFeatureConfig(
      const base::Feature& feature) const override {
    return configuration_.GetFeatureConfig(feature);
  }

  void SetIsCurrentlyShowing(bool is_showing) override {
    is_showing_ = is_showing;
  }

  bool IsCurrentlyShowing() const override { return is_showing_; }

  EditableConfiguration& GetConfiguration() { return configuration_; }

  const Event* GetEvent(const std::string& event_name) const override {
    return nullptr;
  }

  void IncrementEvent(const std::string& event_name, uint32_t day) override {}

 private:
  EditableConfiguration configuration_;
  bool ready_;
  bool is_showing_;
};

class OnceConditionValidatorTest : public ::testing::Test {
 public:
  OnceConditionValidatorTest() {
    // By default, model should be ready.
    model_.SetIsReady(true);
  }

  void AddFeature(const base::Feature& feature, bool valid) {
    FeatureConfig feature_config;
    feature_config.valid = valid;
    model_.GetConfiguration().SetConfiguration(&feature, feature_config);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  TestModel model_;
  OnceConditionValidator validator_;

 private:
  DISALLOW_COPY_AND_ASSIGN(OnceConditionValidatorTest);
};

}  // namespace

TEST_F(OnceConditionValidatorTest, EnabledFeatureShouldTriggerOnce) {
  scoped_feature_list_.InitWithFeatures({kTestFeatureFoo}, {});

  // Initialize validator with one enabled and valid feature.
  AddFeature(kTestFeatureFoo, true);

  // Only the first call to MeetsConditions() should lead to enlightenment.
  EXPECT_TRUE(
      validator_.MeetsConditions(kTestFeatureFoo, model_, 0u).NoErrors());
  ConditionValidator::Result result =
      validator_.MeetsConditions(kTestFeatureFoo, model_, 0u);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.session_rate_ok);
}

TEST_F(OnceConditionValidatorTest,
       BothEnabledAndDisabledFeaturesShouldTrigger) {
  scoped_feature_list_.InitWithFeatures({kTestFeatureFoo}, {kTestFeatureBar});

  // Initialize validator with one enabled and one disabled feature, both valid.
  AddFeature(kTestFeatureFoo, true);
  AddFeature(kTestFeatureBar, true);

  // Only the kTestFeatureFoo feature should lead to enlightenment, since
  // kTestFeatureBar is disabled. Ordering disabled feature first to ensure this
  // captures a different behavior than the
  // OnlyOneFeatureShouldTriggerPerSession test below.
  EXPECT_TRUE(
      validator_.MeetsConditions(kTestFeatureBar, model_, 0u).NoErrors());
  EXPECT_TRUE(
      validator_.MeetsConditions(kTestFeatureFoo, model_, 0u).NoErrors());
}

TEST_F(OnceConditionValidatorTest, StillTriggerWhenAllFeaturesDisabled) {
  scoped_feature_list_.InitWithFeatures({}, {kTestFeatureFoo, kTestFeatureBar});

  // Initialize validator with two enabled features, both valid.
  AddFeature(kTestFeatureFoo, true);
  AddFeature(kTestFeatureBar, true);

  // No features should get to show enlightenment.
  EXPECT_TRUE(
      validator_.MeetsConditions(kTestFeatureFoo, model_, 0u).NoErrors());
  EXPECT_TRUE(
      validator_.MeetsConditions(kTestFeatureBar, model_, 0u).NoErrors());
}

TEST_F(OnceConditionValidatorTest, OnlyTriggerWhenModelIsReady) {
  scoped_feature_list_.InitWithFeatures({kTestFeatureFoo}, {});

  // Initialize validator with a single valid feature.
  AddFeature(kTestFeatureFoo, true);

  model_.SetIsReady(false);
  ConditionValidator::Result result =
      validator_.MeetsConditions(kTestFeatureFoo, model_, 0u);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.model_ready_ok);

  model_.SetIsReady(true);
  EXPECT_TRUE(
      validator_.MeetsConditions(kTestFeatureFoo, model_, 0u).NoErrors());
}

TEST_F(OnceConditionValidatorTest, OnlyTriggerIfNothingElseIsShowing) {
  scoped_feature_list_.InitWithFeatures({kTestFeatureFoo}, {});

  // Initialize validator with a single valid feature.
  AddFeature(kTestFeatureFoo, true);

  model_.SetIsCurrentlyShowing(true);
  ConditionValidator::Result result =
      validator_.MeetsConditions(kTestFeatureFoo, model_, 0u);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.currently_showing_ok);

  model_.SetIsCurrentlyShowing(false);
  EXPECT_TRUE(
      validator_.MeetsConditions(kTestFeatureFoo, model_, 0u).NoErrors());
}

TEST_F(OnceConditionValidatorTest, DoNotTriggerForInvalidConfig) {
  scoped_feature_list_.InitWithFeatures({kTestFeatureFoo}, {});

  AddFeature(kTestFeatureFoo, false);
  ConditionValidator::Result result =
      validator_.MeetsConditions(kTestFeatureFoo, model_, 0u);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.config_ok);

  // Override config to be valid.
  AddFeature(kTestFeatureFoo, true);
  EXPECT_TRUE(
      validator_.MeetsConditions(kTestFeatureFoo, model_, 0u).NoErrors());
}

}  // namespace feature_engagement_tracker
