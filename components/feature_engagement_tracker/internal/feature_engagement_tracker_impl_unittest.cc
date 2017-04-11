// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement_tracker/internal/feature_engagement_tracker_impl.h"

#include <memory>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/test/scoped_feature_list.h"
#include "components/feature_engagement_tracker/internal/editable_configuration.h"
#include "components/feature_engagement_tracker/internal/in_memory_store.h"
#include "components/feature_engagement_tracker/internal/once_condition_validator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feature_engagement_tracker {

namespace {
const base::Feature kTestFeatureFoo{"test_foo",
                                    base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kTestFeatureBar{"test_bar",
                                    base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kTestFeatureQux{"test_qux",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

void RegisterFeatureConfig(EditableConfiguration* configuration,
                           const base::Feature& feature,
                           bool valid) {
  FeatureConfig config;
  config.valid = valid;
  config.feature_used_event = feature.name;
  configuration->SetConfiguration(&feature, config);
}

class FeatureEngagementTrackerImplTest : public ::testing::Test {
 public:
  void SetUp() override {
    std::unique_ptr<Store> store = base::MakeUnique<InMemoryStore>();
    std::unique_ptr<EditableConfiguration> configuration =
        base::MakeUnique<EditableConfiguration>();
    std::unique_ptr<ConditionValidator> validator =
        base::MakeUnique<OnceConditionValidator>();

    RegisterFeatureConfig(configuration.get(), kTestFeatureFoo, true);
    RegisterFeatureConfig(configuration.get(), kTestFeatureBar, true);
    RegisterFeatureConfig(configuration.get(), kTestFeatureQux, false);

    scoped_feature_list_.InitWithFeatures(
        {kTestFeatureFoo, kTestFeatureBar, kTestFeatureQux}, {});

    tracker_.reset(new FeatureEngagementTrackerImpl(
        std::move(store), std::move(configuration), std::move(validator)));
  }

 protected:
  std::unique_ptr<FeatureEngagementTrackerImpl> tracker_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

TEST_F(FeatureEngagementTrackerImplTest, TestTriggering) {
  // The first time a feature triggers it should be shown.
  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTestFeatureFoo));
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTestFeatureFoo));
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTestFeatureQux));

  // While in-product help is currently showing, no other features should be
  // shown.
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTestFeatureBar));
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTestFeatureQux));

  // After dismissing the current in-product help, that feature can not be shown
  // again, but a different feature should.
  tracker_->Dismissed();
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTestFeatureFoo));
  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTestFeatureBar));
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTestFeatureQux));

  // After dismissing the second registered feature, no more in-product help
  // should be shown, since kTestFeatureQux is invalid.
  tracker_->Dismissed();
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTestFeatureFoo));
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTestFeatureBar));
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTestFeatureQux));
}

}  // namespace feature_engagement_tracker
