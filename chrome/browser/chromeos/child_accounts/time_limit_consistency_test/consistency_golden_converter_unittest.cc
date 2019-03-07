// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/time_limit_consistency_test/consistency_golden_converter.h"

#include "base/time/time.h"
#include "chrome/browser/chromeos/child_accounts/time_limit_consistency_test/proto_matcher.h"
#include "chrome/browser/chromeos/child_accounts/time_limit_test_utils.h"
#include "chrome/browser/chromeos/child_accounts/usage_time_limit_processor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace utils = time_limit_test_utils;

namespace time_limit_consistency {

using ConsistencyGoldenConverterTest = testing::Test;

// A timestamp used during the tests. Nothing special about the date it
// points to.
constexpr int64_t kTestTimestamp = 1548709200000L;

TEST_F(ConsistencyGoldenConverterTest, ConvertInputWhenEmpty) {
  ConsistencyGoldenInput input;

  std::unique_ptr<base::DictionaryValue> actual_output =
      ConvertGoldenInputToProcessorInput(input);

  std::unique_ptr<base::DictionaryValue> expected_output =
      utils::CreateTimeLimitPolicy(base::TimeDelta::FromHours(6));

  EXPECT_TRUE(*actual_output == *expected_output);
}

TEST_F(ConsistencyGoldenConverterTest, ConvertInputWithBedtimes) {
  base::Time last_updated = utils::TimeFromString("1 Jan 2018 10:00 GMT+0300");

  ConsistencyGoldenInput input;
  std::unique_ptr<base::DictionaryValue> expected_output =
      utils::CreateTimeLimitPolicy(base::TimeDelta::FromHours(6));

  // First window: Wednesday, 22:30 to 8:00
  ConsistencyGoldenWindowLimitEntry* window_one = input.add_window_limits();
  window_one->mutable_starts_at()->set_hour(22);
  window_one->mutable_starts_at()->set_minute(30);
  window_one->mutable_ends_at()->set_hour(8);
  window_one->mutable_ends_at()->set_minute(0);
  window_one->set_effective_day(WEDNESDAY);
  utils::AddTimeWindowLimit(expected_output.get(), utils::kWednesday,
                            utils::CreateTime(22, 30), utils::CreateTime(8, 0),
                            last_updated);

  // Second window: Saturday, 18:45 to 22:30
  ConsistencyGoldenWindowLimitEntry* window_two = input.add_window_limits();
  window_two->mutable_starts_at()->set_hour(18);
  window_two->mutable_starts_at()->set_minute(45);
  window_two->mutable_ends_at()->set_hour(22);
  window_two->mutable_ends_at()->set_minute(30);
  window_two->set_effective_day(SATURDAY);
  utils::AddTimeWindowLimit(expected_output.get(), utils::kSaturday,
                            utils::CreateTime(18, 45),
                            utils::CreateTime(22, 30), last_updated);

  std::unique_ptr<base::DictionaryValue> actual_output =
      ConvertGoldenInputToProcessorInput(input);

  EXPECT_TRUE(*actual_output == *expected_output);
}

TEST_F(ConsistencyGoldenConverterTest, ConvertOutputWhenUnlocked) {
  usage_time_limit::State state;
  state.is_locked = false;
  state.active_policy = usage_time_limit::ActivePolicies::kNoActivePolicy;
  state.next_unlock_time = base::Time::FromJavaTime(kTestTimestamp);

  ConsistencyGoldenOutput actual_output =
      ConvertProcessorOutputToGoldenOutput(state);

  ConsistencyGoldenOutput expected_output;
  expected_output.set_is_locked(false);
  expected_output.set_active_policy(NO_ACTIVE_POLICY);

  EXPECT_THAT(actual_output, EqualsProto(expected_output));
}

TEST_F(ConsistencyGoldenConverterTest, ConvertOutputWhenLocked) {
  usage_time_limit::State state;
  state.is_locked = true;
  state.active_policy = usage_time_limit::ActivePolicies::kFixedLimit;
  state.next_unlock_time = base::Time::FromJavaTime(kTestTimestamp);

  ConsistencyGoldenOutput actual_output =
      ConvertProcessorOutputToGoldenOutput(state);

  ConsistencyGoldenOutput expected_output;
  expected_output.set_is_locked(true);
  expected_output.set_active_policy(FIXED_LIMIT);
  expected_output.set_next_unlocking_time_millis(kTestTimestamp);

  EXPECT_THAT(actual_output, EqualsProto(expected_output));
}

}  // namespace time_limit_consistency
}  // namespace chromeos
