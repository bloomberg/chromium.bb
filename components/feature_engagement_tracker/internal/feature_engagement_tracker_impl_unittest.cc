// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement_tracker/internal/feature_engagement_tracker_impl.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feature_engagement_tracker {

namespace {

const base::Feature kTestFeatureFoo{"test_foo",
                                    base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kTestFeatureBar{"test_bar",
                                    base::FEATURE_DISABLED_BY_DEFAULT};
}  // namespace

TEST(FeatureEngagementTrackerImplTest, EnabledFeatureShouldTriggerOnce) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kTestFeatureFoo}, {});

  // Initialize tracker with one enabled feature.
  std::vector<const base::Feature*> features = {&kTestFeatureFoo};
  FeatureEngagementTrackerImpl tracker(features);

  // Only the first call to Trigger() should lead to enlightenment.
  EXPECT_TRUE(tracker.Trigger(kTestFeatureFoo));
  EXPECT_FALSE(tracker.Trigger(kTestFeatureFoo));
}

TEST(FeatureEngagementTrackerImplTest, OnlyEnabledFeaturesShouldTrigger) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kTestFeatureFoo}, {kTestFeatureBar});

  // Initialize tracker with one enabled and one disabled feature.
  std::vector<const base::Feature*> features = {&kTestFeatureFoo,
                                                &kTestFeatureBar};
  FeatureEngagementTrackerImpl tracker(features);

  // Only the kTestFeatureFoo feature should lead to enlightenment, since
  // kTestFeatureBar is disabled. Ordering disabled feature first to ensure this
  // captures a different behavior than the
  // OnlyOneFeatureShouldTriggerPerSession test below.
  EXPECT_FALSE(tracker.Trigger(kTestFeatureBar));
  EXPECT_TRUE(tracker.Trigger(kTestFeatureFoo));
}

TEST(FeatureEngagementTrackerImplTest, OnlyOneFeatureShouldTriggerPerSession) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kTestFeatureFoo, kTestFeatureBar}, {});

  // Initialize tracker with two enabled features.
  std::vector<const base::Feature*> features = {&kTestFeatureFoo,
                                                &kTestFeatureBar};
  FeatureEngagementTrackerImpl tracker(features);

  // Only one feature should get to show enlightenment.
  EXPECT_TRUE(tracker.Trigger(kTestFeatureFoo));
  EXPECT_FALSE(tracker.Trigger(kTestFeatureBar));
}

TEST(FeatureEngagementTrackerImplTest, NeverTriggerWhenAllFeaturesDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({}, {kTestFeatureFoo, kTestFeatureBar});

  // Initialize tracker with two enabled features.
  std::vector<const base::Feature*> features = {&kTestFeatureFoo,
                                                &kTestFeatureBar};
  FeatureEngagementTrackerImpl tracker(features);

  // No features should get to show enlightenment.
  EXPECT_FALSE(tracker.Trigger(kTestFeatureFoo));
  EXPECT_FALSE(tracker.Trigger(kTestFeatureBar));
}

}  // namespace feature_engagement_tracker
