// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/time_limit_consistency_test/consistency_golden_converter.h"

#include "base/time/time.h"
#include "chrome/browser/chromeos/child_accounts/time_limit_consistency_test/consistency_test_utils.h"
#include "chrome/browser/chromeos/child_accounts/time_limit_consistency_test/proto_matcher.h"
#include "chrome/browser/chromeos/child_accounts/time_limit_test_utils.h"
#include "chrome/browser/chromeos/child_accounts/usage_time_limit_processor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace utils = time_limit_test_utils;
namespace consistency_utils = time_limit_consistency_utils;

namespace time_limit_consistency {

using ConsistencyGoldenConverterTest = testing::Test;

// A timestamp used during the tests. Nothing special about the date it
// points to.
constexpr int64_t kTestTimestamp = 1548709200000L;

// An arbitrary date representing the last time the policy was updated. Used on
// tests where such information is required but irrelevant to the test.
const base::Time kTestLastUpdated =
    utils::TimeFromString("1 Jan 2018 10:00 GMT+0300");

TEST_F(ConsistencyGoldenConverterTest, ConvertInputWhenEmpty) {
  ConsistencyGoldenInput input;

  std::unique_ptr<base::DictionaryValue> actual_output =
      ConvertGoldenInputToProcessorInput(input);

  std::unique_ptr<base::DictionaryValue> expected_output =
      utils::CreateTimeLimitPolicy(base::TimeDelta::FromHours(6));

  EXPECT_TRUE(*actual_output == *expected_output);
}

TEST_F(ConsistencyGoldenConverterTest, ConvertInputWithBedtimes) {
  ConsistencyGoldenInput input;
  std::unique_ptr<base::DictionaryValue> expected_output =
      utils::CreateTimeLimitPolicy(base::TimeDelta::FromHours(6));

  // First window: Wednesday, 22:30 to 8:00
  consistency_utils::AddWindowLimitEntryToGoldenInput(&input, WEDNESDAY, 22, 30,
                                                      8, 0);
  utils::AddTimeWindowLimit(expected_output.get(), utils::kWednesday,
                            utils::CreateTime(22, 30), utils::CreateTime(8, 0),
                            kTestLastUpdated);

  // Second window: Saturday, 18:45 to 22:30
  consistency_utils::AddWindowLimitEntryToGoldenInput(&input, SATURDAY, 18, 45,
                                                      22, 30);
  utils::AddTimeWindowLimit(expected_output.get(), utils::kSaturday,
                            utils::CreateTime(18, 45),
                            utils::CreateTime(22, 30), kTestLastUpdated);

  std::unique_ptr<base::DictionaryValue> actual_output =
      ConvertGoldenInputToProcessorInput(input);

  EXPECT_TRUE(*actual_output == *expected_output);
}

TEST_F(ConsistencyGoldenConverterTest, ConvertInputWithUsageLimit) {
  ConsistencyGoldenInput input;
  std::unique_ptr<base::DictionaryValue> expected_output =
      utils::CreateTimeLimitPolicy(utils::CreateTime(17, 30));

  input.mutable_usage_limit_resets_at()->set_hour(17);
  input.mutable_usage_limit_resets_at()->set_minute(30);

  // First quota: Tuesday, 60 minutes
  consistency_utils::AddUsageLimitEntryToGoldenInput(&input, TUESDAY, 60);
  utils::AddTimeUsageLimit(expected_output.get(), utils::kTuesday,
                           base::TimeDelta::FromMinutes(60), kTestLastUpdated);

  // Second quota: Friday, 30 minutes
  consistency_utils::AddUsageLimitEntryToGoldenInput(&input, FRIDAY, 30);
  utils::AddTimeUsageLimit(expected_output.get(), utils::kFriday,
                           base::TimeDelta::FromMinutes(30), kTestLastUpdated);

  std::unique_ptr<base::DictionaryValue> actual_output =
      ConvertGoldenInputToProcessorInput(input);

  EXPECT_TRUE(*actual_output == *expected_output);
}

TEST_F(ConsistencyGoldenConverterTest, ConvertInputWithUsageLimitDefaultReset) {
  ConsistencyGoldenInput input;
  std::unique_ptr<base::DictionaryValue> expected_output =
      utils::CreateTimeLimitPolicy(base::TimeDelta::FromHours(6));

  // First quota: Tuesday, 60 minutes
  consistency_utils::AddUsageLimitEntryToGoldenInput(&input, TUESDAY, 60);
  utils::AddTimeUsageLimit(expected_output.get(), utils::kTuesday,
                           base::TimeDelta::FromMinutes(60), kTestLastUpdated);

  // Second quota: Friday, 30 minutes
  consistency_utils::AddUsageLimitEntryToGoldenInput(&input, FRIDAY, 30);
  utils::AddTimeUsageLimit(expected_output.get(), utils::kFriday,
                           base::TimeDelta::FromMinutes(30), kTestLastUpdated);

  std::unique_ptr<base::DictionaryValue> actual_output =
      ConvertGoldenInputToProcessorInput(input);

  EXPECT_TRUE(*actual_output == *expected_output);
}

TEST_F(ConsistencyGoldenConverterTest, ConvertOutputWhenUnlocked) {
  usage_time_limit::State state;
  state.is_locked = false;
  state.active_policy = usage_time_limit::ActivePolicies::kNoActivePolicy;
  state.next_state_active_policy =
      usage_time_limit::ActivePolicies::kNoActivePolicy;
  state.next_unlock_time = base::Time::FromJavaTime(kTestTimestamp);

  ConsistencyGoldenOutput actual_output =
      ConvertProcessorOutputToGoldenOutput(state);

  ConsistencyGoldenOutput expected_output;
  expected_output.set_is_locked(false);
  expected_output.set_active_policy(NO_ACTIVE_POLICY);
  expected_output.set_next_active_policy(NO_ACTIVE_POLICY);

  EXPECT_THAT(actual_output, EqualsProto(expected_output));
}

TEST_F(ConsistencyGoldenConverterTest, ConvertOutputWhenLockedByBedtime) {
  usage_time_limit::State state;
  state.is_locked = true;
  state.active_policy = usage_time_limit::ActivePolicies::kFixedLimit;
  state.next_state_active_policy =
      usage_time_limit::ActivePolicies::kNoActivePolicy;
  state.next_unlock_time = base::Time::FromJavaTime(kTestTimestamp);

  ConsistencyGoldenOutput actual_output =
      ConvertProcessorOutputToGoldenOutput(state);

  ConsistencyGoldenOutput expected_output;
  expected_output.set_is_locked(true);
  expected_output.set_active_policy(FIXED_LIMIT);
  expected_output.set_next_active_policy(NO_ACTIVE_POLICY);
  expected_output.set_next_unlocking_time_millis(kTestTimestamp);

  EXPECT_THAT(actual_output, EqualsProto(expected_output));
}

TEST_F(ConsistencyGoldenConverterTest, ConvertOutputWhenLockedByUsageLimit) {
  int remaining_millis = 10000;

  usage_time_limit::State state;
  state.is_locked = true;
  state.active_policy = usage_time_limit::ActivePolicies::kUsageLimit;
  state.next_state_active_policy =
      usage_time_limit::ActivePolicies::kNoActivePolicy;
  state.is_time_usage_limit_enabled = true;
  state.remaining_usage = base::TimeDelta::FromMilliseconds(remaining_millis);
  state.next_unlock_time = base::Time::FromJavaTime(kTestTimestamp);

  ConsistencyGoldenOutput actual_output =
      ConvertProcessorOutputToGoldenOutput(state);

  ConsistencyGoldenOutput expected_output;
  expected_output.set_is_locked(true);
  expected_output.set_active_policy(USAGE_LIMIT);
  expected_output.set_next_active_policy(NO_ACTIVE_POLICY);
  expected_output.set_remaining_quota_millis(remaining_millis);
  expected_output.set_next_unlocking_time_millis(kTestTimestamp);

  EXPECT_THAT(actual_output, EqualsProto(expected_output));
}

}  // namespace time_limit_consistency
}  // namespace chromeos
