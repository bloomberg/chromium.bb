// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement_tracker/internal/never_condition_validator.h"

#include <string>

#include "base/feature_list.h"
#include "components/feature_engagement_tracker/internal/configuration.h"
#include "components/feature_engagement_tracker/internal/model.h"
#include "components/feature_engagement_tracker/internal/proto/event.pb.h"
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
  TestModel() = default;

  void Initialize(const OnModelInitializationFinished& callback,
                  uint32_t current_day) override {}

  bool IsReady() const override { return true; }

  const Event* GetEvent(const std::string& event_name) const override {
    return nullptr;
  }

  void IncrementEvent(const std::string& event_name, uint32_t day) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestModel);
};

class NeverConditionValidatorTest : public ::testing::Test {
 public:
  NeverConditionValidatorTest() = default;

 protected:
  TestModel model_;
  NeverConditionValidator validator_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NeverConditionValidatorTest);
};

}  // namespace

TEST_F(NeverConditionValidatorTest, ShouldNeverMeetConditions) {
  EXPECT_FALSE(
      validator_.MeetsConditions(kTestFeatureFoo, FeatureConfig(), model_, 0u)
          .NoErrors());
  EXPECT_FALSE(
      validator_.MeetsConditions(kTestFeatureBar, FeatureConfig(), model_, 0u)
          .NoErrors());
}

}  // namespace feature_engagement_tracker
