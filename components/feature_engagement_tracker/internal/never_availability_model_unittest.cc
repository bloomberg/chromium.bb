// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement_tracker/internal/never_availability_model.h"

#include "base/feature_list.h"
#include "base/optional.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feature_engagement_tracker {

namespace {

const base::Feature kTestFeatureFoo{"test_foo",
                                    base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kTestFeatureBar{"test_bar",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

class NeverAvailabilityModelTest : public ::testing::Test {
 public:
  NeverAvailabilityModelTest() = default;

 protected:
  NeverAvailabilityModel availability_model_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NeverAvailabilityModelTest);
};

}  // namespace

TEST_F(NeverAvailabilityModelTest, ShouldNeverHaveData) {
  EXPECT_EQ(base::nullopt,
            availability_model_.GetAvailability(kTestFeatureFoo));
  EXPECT_EQ(base::nullopt,
            availability_model_.GetAvailability(kTestFeatureBar));
}

}  // namespace feature_engagement_tracker
