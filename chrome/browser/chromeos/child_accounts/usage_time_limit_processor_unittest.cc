// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/usage_time_limit_processor.h"

#include <memory>
#include <string>
#include <utility>

#include "base/values.h"
#include "chrome/browser/chromeos/child_accounts/time_limit_test_utils.h"
#include "chromeos/settings/timezone_settings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace utils = time_limit_test_utils;

namespace usage_time_limit {

using UsageTimeLimitProcessorTest = testing::Test;

void AssertEqState(State expected, State actual) {
  ASSERT_EQ(expected.is_locked, actual.is_locked);
  ASSERT_EQ(expected.active_policy, actual.active_policy);
  ASSERT_EQ(expected.is_time_usage_limit_enabled,
            actual.is_time_usage_limit_enabled);

  if (actual.is_time_usage_limit_enabled) {
    ASSERT_EQ(expected.remaining_usage, actual.remaining_usage);
    if (actual.remaining_usage <= base::TimeDelta::FromMinutes(0)) {
      ASSERT_EQ(expected.time_usage_limit_started,
                actual.time_usage_limit_started);
    }
  }

  ASSERT_EQ(expected.next_state_change_time, actual.next_state_change_time);
  ASSERT_EQ(expected.next_state_active_policy, actual.next_state_active_policy);

  if (actual.is_locked)
    ASSERT_EQ(expected.next_unlock_time, actual.next_unlock_time);
}

namespace internal {

using UsageTimeLimitProcessorInternalTest = testing::Test;

TEST_F(UsageTimeLimitProcessorInternalTest, TimeLimitWindowValid) {
  base::Time last_updated = utils::TimeFromString("1 Jan 1970 00:00:00");
  base::Value monday_time_limit = utils::CreateTimeWindow(
      utils::kMonday, base::TimeDelta::FromMinutes(22 * 60 + 30),
      base::TimeDelta::FromMinutes(7 * 60 + 30), last_updated);
  base::Value friday_time_limit = utils::CreateTimeWindow(
      utils::kFriday, base::TimeDelta::FromHours(23),
      base::TimeDelta::FromMinutes(8 * 60 + 20), last_updated);

  base::Value window_limit_entries(base::Value::Type::LIST);
  window_limit_entries.GetList().push_back(std::move(monday_time_limit));
  window_limit_entries.GetList().push_back(std::move(friday_time_limit));

  base::Value time_window_limit = base::Value(base::Value::Type::DICTIONARY);
  time_window_limit.SetKey("entries", std::move(window_limit_entries));

  // Call tested function.
  TimeWindowLimit window_limit_struct(time_window_limit);

  ASSERT_TRUE(window_limit_struct.entries[Weekday::kMonday]);
  ASSERT_EQ(window_limit_struct.entries[Weekday::kMonday]
                .value()
                .starts_at.InMinutes(),
            22 * 60 + 30);
  ASSERT_EQ(
      window_limit_struct.entries[Weekday::kMonday].value().ends_at.InMinutes(),
      7 * 60 + 30);
  ASSERT_EQ(window_limit_struct.entries[Weekday::kMonday].value().last_updated,
            base::Time::UnixEpoch());

  ASSERT_TRUE(window_limit_struct.entries[Weekday::kFriday]);
  ASSERT_EQ(window_limit_struct.entries[Weekday::kFriday]
                .value()
                .starts_at.InMinutes(),
            23 * 60);
  ASSERT_EQ(
      window_limit_struct.entries[Weekday::kFriday].value().ends_at.InMinutes(),
      8 * 60 + 20);
  ASSERT_EQ(window_limit_struct.entries[Weekday::kFriday].value().last_updated,
            base::Time::UnixEpoch());

  // Assert that weekdays without time_window_limits are not set.
  ASSERT_FALSE(window_limit_struct.entries[Weekday::kTuesday]);
  ASSERT_FALSE(window_limit_struct.entries[Weekday::kWednesday]);
  ASSERT_FALSE(window_limit_struct.entries[Weekday::kThursday]);
  ASSERT_FALSE(window_limit_struct.entries[Weekday::kSaturday]);
  ASSERT_FALSE(window_limit_struct.entries[Weekday::kSunday]);
}

// Validates that a well formed dictionary containing the time_usage_limit
// information from the UsageTimeLimit policy is converted to its intermediate
// representation correctly.
TEST_F(UsageTimeLimitProcessorInternalTest, TimeUsageWindowValid) {
  // Create dictionary containing the policy information.
  base::Time last_updated_one = utils::TimeFromString("1 Jan 2018 10:00:00");
  base::Time last_updated_two = utils::TimeFromString("1 Jan 2018 11:00:00");
  base::Value tuesday_time_usage = utils::CreateTimeUsage(
      base::TimeDelta::FromMinutes(120), last_updated_one);
  base::Value thursday_time_usage = utils::CreateTimeUsage(
      base::TimeDelta::FromMinutes(80), last_updated_two);

  base::Value time_usage_limit = base::Value(base::Value::Type::DICTIONARY);
  time_usage_limit.SetKey("tuesday", std::move(tuesday_time_usage));
  time_usage_limit.SetKey("thursday", std::move(thursday_time_usage));
  time_usage_limit.SetKey("reset_at",
                          utils::CreatePolicyTime(utils::CreateTime(8, 0)));

  // Call tested functions.
  TimeUsageLimit usage_limit_struct(time_usage_limit);

  ASSERT_EQ(usage_limit_struct.resets_at.InMinutes(), 8 * 60);

  ASSERT_EQ(usage_limit_struct.entries[Weekday::kTuesday]
                .value()
                .usage_quota.InMinutes(),
            120);
  ASSERT_EQ(usage_limit_struct.entries[Weekday::kTuesday].value().last_updated,
            base::Time::FromDoubleT(1514800800));

  ASSERT_EQ(usage_limit_struct.entries[Weekday::kThursday]
                .value()
                .usage_quota.InMinutes(),
            80);
  ASSERT_EQ(usage_limit_struct.entries[Weekday::kThursday].value().last_updated,
            base::Time::FromDoubleT(1514804400));

  // Assert that weekdays without time_usage_limits are not set.
  ASSERT_FALSE(usage_limit_struct.entries[Weekday::kMonday]);
  ASSERT_FALSE(usage_limit_struct.entries[Weekday::kWednesday]);
  ASSERT_FALSE(usage_limit_struct.entries[Weekday::kFriday]);
  ASSERT_FALSE(usage_limit_struct.entries[Weekday::kSaturday]);
  ASSERT_FALSE(usage_limit_struct.entries[Weekday::kSunday]);
}

// Validates that a well formed dictionary containing the override information
// from the UsageTimeLimit policy is converted to its intermediate
// representation correctly.
TEST_F(UsageTimeLimitProcessorInternalTest, OverrideValid) {
  // Create policy information.
  std::string created_at_millis =
      utils::CreatePolicyTimestamp("1 Jan 2018 10:00:00");
  base::Value override_one = base::Value(base::Value::Type::DICTIONARY);
  override_one.SetKey("action", base::Value(utils::kUnlock));
  override_one.SetKey("created_at_millis", base::Value(created_at_millis));

  base::Value override_two = base::Value(base::Value::Type::DICTIONARY);
  override_two.SetKey("action", base::Value(utils::kLock));
  override_two.SetKey(
      "created_at_millis",
      base::Value(utils::CreatePolicyTimestamp("1 Jan 2018 9:00:00")));

  base::Value overrides(base::Value::Type::LIST);
  overrides.GetList().push_back(std::move(override_one));
  overrides.GetList().push_back(std::move(override_two));

  // Call tested functions.
  TimeLimitOverride override_struct(overrides);

  // Assert right fields are set.
  ASSERT_EQ(override_struct.action, TimeLimitOverride::Action::kUnlock);
  ASSERT_EQ(override_struct.created_at,
            utils::TimeFromString("1 Jan 2018 10:00:00"));
  ASSERT_FALSE(override_struct.duration);
}

// Check that the most recent override is chosen when more than one is sent on
// the policy. This test covers the corner case when the timestamp strings have
// different sizes.
TEST_F(UsageTimeLimitProcessorInternalTest, MultipleOverrides) {
  // Create policy information.
  base::Value override_one = base::Value(base::Value::Type::DICTIONARY);
  override_one.SetKey("action", base::Value(utils::kUnlock));
  override_one.SetKey("created_at_millis", base::Value("1000000"));

  base::Value override_two = base::Value(base::Value::Type::DICTIONARY);
  override_two.SetKey("action", base::Value(utils::kLock));
  override_two.SetKey("created_at_millis", base::Value("999999"));

  base::Value override_three = base::Value(base::Value::Type::DICTIONARY);
  override_two.SetKey("action", base::Value(utils::kLock));
  override_two.SetKey("created_at_millis", base::Value("900000"));

  base::Value override_four = base::Value(base::Value::Type::DICTIONARY);
  override_two.SetKey("action", base::Value(utils::kUnlock));
  override_two.SetKey("created_at_millis", base::Value("1200000"));

  base::Value overrides(base::Value::Type::LIST);
  overrides.GetList().push_back(std::move(override_one));
  overrides.GetList().push_back(std::move(override_two));
  overrides.GetList().push_back(std::move(override_three));
  overrides.GetList().push_back(std::move(override_four));

  // Call tested functions.
  TimeLimitOverride override_struct(overrides);

  // Assert right fields are set.
  ASSERT_EQ(override_struct.action, TimeLimitOverride::Action::kUnlock);
  ASSERT_EQ(override_struct.created_at, base::Time::FromJavaTime(1200000));
  ASSERT_FALSE(override_struct.duration);
}

}  // namespace internal

// Tests the GetState for a policy that only have the time window limit set. It
// is checked that the state is correct before, during and after the policy is
// enforced.
TEST_F(UsageTimeLimitProcessorTest, GetStateOnlyTimeWindowLimitSet) {
  std::unique_ptr<icu::TimeZone> timezone(
      icu::TimeZone::createTimeZone("GMT+0300"));

  // Set up policy.
  base::Time last_updated = utils::TimeFromString("1 Jan 2018 10:00 GMT+0300");
  std::unique_ptr<base::DictionaryValue> policy =
      utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));

  utils::AddTimeWindowLimit(policy.get(), utils::kSunday,
                            utils::CreateTime(22, 0), utils::CreateTime(7, 30),
                            last_updated);
  utils::AddTimeWindowLimit(policy.get(), utils::kMonday,
                            utils::CreateTime(21, 0), utils::CreateTime(7, 30),
                            last_updated);
  utils::AddTimeWindowLimit(policy.get(), utils::kTuesday,
                            utils::CreateTime(7, 30), utils::CreateTime(9, 0),
                            last_updated);
  utils::AddTimeWindowLimit(policy.get(), utils::kFriday,
                            utils::CreateTime(21, 0), utils::CreateTime(7, 30),
                            last_updated);

  base::Time monday_time_window_limit_start =
      utils::TimeFromString("Mon, 1 Jan 2018 21:00 GMT+0300");
  base::Time monday_time_window_limit_end =
      utils::TimeFromString("Tue, 2 Jan 2018 7:30 GMT+0300");
  base::Time tuesday_time_window_limit_end =
      utils::TimeFromString("Tue, 2 Jan 2018 9:00 GMT+0300");
  base::Time friday_time_window_limit_start =
      utils::TimeFromString("Fri, 5 Jan 2018 21:00 GMT+0300");

  // Check state before Monday time window limit.
  base::Time time_one = utils::TimeFromString("Mon, 1 Jan 2018 20:00 GMT+0300");
  State state_one = GetState(policy, base::TimeDelta::FromMinutes(0), time_one,
                             time_one, timezone.get(), base::nullopt);

  State expected_state_one;
  expected_state_one.is_locked = false;
  expected_state_one.active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_one.is_time_usage_limit_enabled = false;
  expected_state_one.next_state_change_time = monday_time_window_limit_start;
  expected_state_one.next_state_active_policy = ActivePolicies::kFixedLimit;

  AssertEqState(expected_state_one, state_one);

  // Check state during the Monday time window limit.
  base::Time time_two = utils::TimeFromString("Mon, 1 Jan 2018 22:00 GMT+0300");
  State state_two = GetState(policy, base::TimeDelta::FromMinutes(0), time_two,
                             time_two, timezone.get(), state_one);

  State expected_state_two;
  expected_state_two.is_locked = true;
  expected_state_two.active_policy = ActivePolicies::kFixedLimit;
  expected_state_two.is_time_usage_limit_enabled = false;
  expected_state_two.next_state_change_time = monday_time_window_limit_end;
  expected_state_two.next_state_active_policy = ActivePolicies::kFixedLimit;
  expected_state_two.next_unlock_time = tuesday_time_window_limit_end;

  AssertEqState(expected_state_two, state_two);

  // Check state after the Monday time window limit.
  base::Time time_three =
      utils::TimeFromString("Tue, 2 Jan 2018 9:00 GMT+0300");
  State state_three =
      GetState(policy, base::TimeDelta::FromMinutes(0), time_three, time_three,
               timezone.get(), state_two);

  State expected_state_three;
  expected_state_three.is_locked = false;
  expected_state_three.active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_three.is_time_usage_limit_enabled = false;
  expected_state_three.next_state_change_time = friday_time_window_limit_start;
  expected_state_three.next_state_active_policy = ActivePolicies::kFixedLimit;

  AssertEqState(expected_state_three, state_three);
}

// Tests the GetState for a policy that only have the time usage limit set. It
// is checked that the state is correct before and during the policy is
// enforced.
TEST_F(UsageTimeLimitProcessorTest, GetStateOnlyTimeUsageLimitSet) {
  std::unique_ptr<icu::TimeZone> timezone(icu::TimeZone::createTimeZone("GMT"));

  // Set up policy.
  base::Time last_updated = utils::TimeFromString("1 Jan 2018 8:00");
  std::unique_ptr<base::DictionaryValue> policy =
      utils::CreateTimeLimitPolicy(utils::CreateTime(8, 0));

  utils::AddTimeUsageLimit(policy.get(), utils::kTuesday,
                           base::TimeDelta::FromHours(2), last_updated);
  utils::AddTimeUsageLimit(policy.get(), utils::kThursday,
                           base::TimeDelta::FromMinutes(80), last_updated);

  // Check state before time usage limit is enforced.
  base::Time time_one = utils::TimeFromString("Mon, 1 Jan 2018 20:00");
  State state_one = GetState(policy, base::TimeDelta::FromMinutes(120),
                             time_one, time_one, timezone.get(), base::nullopt);

  State expected_state_one;
  expected_state_one.is_locked = false;
  expected_state_one.active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_one.is_time_usage_limit_enabled = false;
  // Next state is the minimum time when the time usage limit could be enforced.
  expected_state_one.next_state_change_time =
      utils::TimeFromString("Tue, 2 Jan 2018 10:00");
  expected_state_one.next_state_active_policy = ActivePolicies::kUsageLimit;

  AssertEqState(expected_state_one, state_one);

  // Check state before time usage limit is enforced.
  base::Time time_two = utils::TimeFromString("Tue, 2 Jan 2018 12:00");
  State state_two = GetState(policy, base::TimeDelta::FromMinutes(60), time_two,
                             time_two, timezone.get(), state_one);

  State expected_state_two;
  expected_state_two.is_locked = false;
  expected_state_two.active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_two.is_time_usage_limit_enabled = true;
  expected_state_two.remaining_usage = base::TimeDelta::FromMinutes(60);
  expected_state_two.next_state_change_time =
      time_two + base::TimeDelta::FromMinutes(60);
  expected_state_two.next_state_active_policy = ActivePolicies::kUsageLimit;

  AssertEqState(expected_state_two, state_two);

  // Check state when the time usage limit should be enforced.
  base::Time time_three = utils::TimeFromString("Tue, 2 Jan 2018 21:00");
  State state_three =
      GetState(policy, base::TimeDelta::FromMinutes(120), time_three,
               time_three, timezone.get(), state_two);

  base::Time wednesday_reset_time =
      utils::TimeFromString("Wed, 3 Jan 2018 8:00");

  State expected_state_three;
  expected_state_three.is_locked = true;
  expected_state_three.active_policy = ActivePolicies::kUsageLimit;
  expected_state_three.is_time_usage_limit_enabled = true;
  expected_state_three.remaining_usage = base::TimeDelta::FromMinutes(0);
  expected_state_three.time_usage_limit_started = time_three;
  expected_state_three.next_state_change_time = wednesday_reset_time;
  expected_state_three.next_state_active_policy =
      ActivePolicies::kNoActivePolicy;
  expected_state_three.next_unlock_time = wednesday_reset_time;

  AssertEqState(expected_state_three, state_three);
}

// Test GetState with both time window limit and time usage limit defined.
TEST_F(UsageTimeLimitProcessorTest, GetStateWithTimeUsageAndWindowLimitActive) {
  std::unique_ptr<icu::TimeZone> timezone(icu::TimeZone::createTimeZone("GMT"));

  // Setup policy.
  base::Time last_updated = utils::TimeFromString("1 Jan 2018 8:00");
  std::unique_ptr<base::DictionaryValue> policy =
      utils::CreateTimeLimitPolicy(utils::CreateTime(8, 0));

  utils::AddTimeWindowLimit(policy.get(), utils::kMonday,
                            utils::CreateTime(21, 0), utils::CreateTime(8, 30),
                            last_updated);
  utils::AddTimeWindowLimit(policy.get(), utils::kFriday,
                            utils::CreateTime(21, 0), utils::CreateTime(8, 30),
                            last_updated);

  utils::AddTimeUsageLimit(policy.get(), utils::kMonday,
                           base::TimeDelta::FromHours(2), last_updated);

  // Check state before any policy is enforced.
  base::Time time_one = utils::TimeFromString("Mon, 1 Jan 2018 14:00");
  State state_one = GetState(policy, base::TimeDelta::FromMinutes(80), time_one,
                             time_one, timezone.get(), base::nullopt);

  State expected_state_one;
  expected_state_one.is_locked = false;
  expected_state_one.active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_one.is_time_usage_limit_enabled = true;
  expected_state_one.remaining_usage = base::TimeDelta::FromMinutes(40);
  expected_state_one.next_state_change_time =
      time_one + base::TimeDelta::FromMinutes(40);
  expected_state_one.next_state_active_policy = ActivePolicies::kUsageLimit;

  AssertEqState(expected_state_one, state_one);

  // Check state during time usage limit.
  base::Time time_two = utils::TimeFromString("Mon, 1 Jan 2018 16:00");
  State state_two = GetState(policy, base::TimeDelta::FromMinutes(121),
                             time_two, time_two, timezone.get(), state_one);

  base::Time monday_time_window_limit_start =
      utils::TimeFromString("Mon, 1 Jan 2018 21:00");

  State expected_state_two;
  expected_state_two.is_locked = true;
  expected_state_two.active_policy = ActivePolicies::kUsageLimit;
  expected_state_two.is_time_usage_limit_enabled = true;
  expected_state_two.remaining_usage = base::TimeDelta::FromMinutes(0);
  expected_state_two.time_usage_limit_started = time_two;
  expected_state_two.next_state_change_time = monday_time_window_limit_start;
  expected_state_two.next_state_active_policy = ActivePolicies::kFixedLimit;
  expected_state_two.next_unlock_time =
      utils::TimeFromString("Tue, 2 Jan 2018 8:30");

  AssertEqState(expected_state_two, state_two);

  // Check state during time window limit and time usage limit enforced.
  base::Time time_three = utils::TimeFromString("Mon, 1 Jan 2018 21:00");
  State state_three =
      GetState(policy, base::TimeDelta::FromMinutes(120), time_three,
               time_three, timezone.get(), state_two);

  State expected_state_three;
  expected_state_three.is_locked = true;
  expected_state_three.active_policy = ActivePolicies::kFixedLimit;
  expected_state_three.is_time_usage_limit_enabled = true;
  expected_state_three.remaining_usage = base::TimeDelta::FromMinutes(0);
  expected_state_three.time_usage_limit_started = time_two;
  expected_state_three.next_state_change_time =
      utils::TimeFromString("Tue, 2 Jan 2018 8:30");
  expected_state_three.next_state_active_policy =
      ActivePolicies::kNoActivePolicy;
  expected_state_three.next_unlock_time =
      utils::TimeFromString("Tue, 2 Jan 2018 8:30");

  AssertEqState(expected_state_three, state_three);

  // Check state after time usage limit reset and window limit end.
  base::Time time_four = utils::TimeFromString("Fri, 5 Jan 2018 8:30");
  State state_four =
      GetState(policy, base::TimeDelta::FromMinutes(120), time_four, time_four,
               timezone.get(), state_three);

  State expected_state_four;
  expected_state_four.is_locked = false;
  expected_state_four.active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_four.is_time_usage_limit_enabled = false;
  expected_state_four.next_state_change_time =
      utils::TimeFromString("Fri, 5 Jan 2018 21:00");
  expected_state_four.next_state_active_policy = ActivePolicies::kFixedLimit;

  AssertEqState(expected_state_four, state_four);
}

// Test time usage limit lock without previous state.
TEST_F(UsageTimeLimitProcessorTest, GetStateFirstExecutionLockByUsageLimit) {
  std::unique_ptr<icu::TimeZone> timezone(icu::TimeZone::createTimeZone("PST"));

  // Setup policy.
  base::Time last_updated = utils::TimeFromString("5 Jan 2018 8:00 PST");
  std::unique_ptr<base::DictionaryValue> policy =
      utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));
  utils::AddTimeUsageLimit(policy.get(), utils::kFriday,
                           base::TimeDelta::FromHours(1), last_updated);

  base::Time time_one = utils::TimeFromString("Fri, 5 Jan 2018 15:00 PST");
  State state_one = GetState(policy, base::TimeDelta::FromHours(1), time_one,
                             time_one, timezone.get(), base::nullopt);

  State expected_state_one;
  expected_state_one.is_locked = true;
  expected_state_one.active_policy = ActivePolicies::kUsageLimit;
  expected_state_one.is_time_usage_limit_enabled = true;
  expected_state_one.remaining_usage = base::TimeDelta::FromMinutes(0);
  expected_state_one.next_state_change_time =
      utils::TimeFromString("Sat, 6 Jan 2018 6:00 PST");
  expected_state_one.next_state_active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_one.time_usage_limit_started = time_one;
  expected_state_one.next_unlock_time =
      utils::TimeFromString("Sat, 6 Jan 2018 6:00 PST");

  AssertEqState(expected_state_one, state_one);
}

// Check GetState when a lock override is active.
TEST_F(UsageTimeLimitProcessorTest, GetStateWithOverrideLock) {
  std::unique_ptr<icu::TimeZone> timezone(icu::TimeZone::createTimeZone("GMT"));

  std::unique_ptr<base::DictionaryValue> policy =
      std::make_unique<base::DictionaryValue>(base::DictionaryValue());
  utils::AddOverride(policy.get(), utils::kLock,
                     utils::TimeFromString("Mon, 1 Jan 2018 15:00"));

  base::Time time_one = utils::TimeFromString("Mon, 1 Jan 2018 15:05");
  State state_one = GetState(policy, base::TimeDelta::FromMinutes(0), time_one,
                             time_one, timezone.get(), base::nullopt);

  // Check that the device is locked until next morning.
  State expected_state_one;
  expected_state_one.is_locked = true;
  expected_state_one.active_policy = ActivePolicies::kOverride;
  expected_state_one.is_time_usage_limit_enabled = false;
  expected_state_one.next_state_change_time =
      utils::TimeFromString("Tue, 2 Jan 2018 0:00");
  expected_state_one.next_state_active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_one.next_unlock_time =
      utils::TimeFromString("Tue, 2 Jan 2018 0:00");

  AssertEqState(expected_state_one, state_one);
}

// Test GetState when a overridden time window limit has been updated, so the
// override should not be aplicable anymore.
TEST_F(UsageTimeLimitProcessorTest, GetStateUpdateUnlockedTimeWindowLimit) {
  std::unique_ptr<icu::TimeZone> timezone(
      icu::TimeZone::createTimeZone("GMT+0800"));

  // Setup policy.
  base::Time last_updated =
      utils::TimeFromString("Mon, 1 Jan 2018 8:00 GMT+0800");
  std::unique_ptr<base::DictionaryValue> policy =
      std::make_unique<base::DictionaryValue>(base::DictionaryValue());
  utils::AddTimeWindowLimit(policy.get(), utils::kMonday,
                            utils::CreateTime(18, 0), utils::CreateTime(7, 30),
                            last_updated);
  utils::AddOverride(policy.get(), utils::kUnlock,
                     utils::TimeFromString("Mon, 1 Jan 2018 18:30 GMT+0800"));

  // Check that the override is invalidating the time window limit.
  base::Time time_one = utils::TimeFromString("Mon, 1 Jan 2018 18:35 GMT+0800");
  State state_one = GetState(policy, base::TimeDelta::FromMinutes(120),
                             time_one, time_one, timezone.get(), base::nullopt);

  State expected_state_one;
  expected_state_one.is_locked = false;
  expected_state_one.active_policy = ActivePolicies::kOverride;
  expected_state_one.is_time_usage_limit_enabled = false;
  expected_state_one.next_state_change_time =
      utils::TimeFromString("Mon, 8 Jan 2018 18:00 GMT+0800");
  expected_state_one.next_state_active_policy = ActivePolicies::kFixedLimit;

  AssertEqState(expected_state_one, state_one);

  // Change time window limit
  base::Time last_updated_two =
      utils::TimeFromString("Mon, 1 Jan 2018 19:00 GMT+0800");
  utils::AddTimeWindowLimit(policy.get(), utils::kMonday,
                            utils::CreateTime(18, 0), utils::CreateTime(8, 0),
                            last_updated_two);

  // Check that the new time window limit is enforced.
  base::Time time_two = utils::TimeFromString("Mon, 1 Jan 2018 19:10 GMT+0800");
  State state_two = GetState(policy, base::TimeDelta::FromMinutes(120),
                             time_two, time_two, timezone.get(), state_one);

  State expected_state_two;
  expected_state_two.is_locked = true;
  expected_state_two.active_policy = ActivePolicies::kFixedLimit;
  expected_state_two.is_time_usage_limit_enabled = false;
  expected_state_two.next_state_change_time =
      utils::TimeFromString("Tue, 2 Jan 2018 8:00 GMT+0800");
  expected_state_two.next_state_active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_two.next_unlock_time =
      utils::TimeFromString("Tue, 2 Jan 2018 8:00 GMT+0800");

  AssertEqState(expected_state_two, state_two);
}

// Make sure that the override will only affect policies that started being
// enforced before it was created.
TEST_F(UsageTimeLimitProcessorTest, GetStateOverrideTimeWindowLimitOnly) {
  std::unique_ptr<icu::TimeZone> timezone(icu::TimeZone::createTimeZone("PST"));

  // Setup policy.
  base::Time last_updated = utils::TimeFromString("1 Jan 2018 8:00 PST");
  std::unique_ptr<base::DictionaryValue> policy =
      utils::CreateTimeLimitPolicy(utils::CreateTime(8, 0));
  utils::AddTimeWindowLimit(policy.get(), utils::kMonday,
                            utils::CreateTime(21, 0), utils::CreateTime(10, 0),
                            last_updated);
  utils::AddTimeUsageLimit(policy.get(), utils::kMonday,
                           base::TimeDelta::FromHours(1), last_updated);
  utils::AddOverride(policy.get(), utils::kUnlock,
                     utils::TimeFromString("Mon, 1 Jan 2018 22:00 PST"));

  base::Time time_one = utils::TimeFromString("Mon, 1 Jan 2018 22:10 PST");
  State state_one = GetState(policy, base::TimeDelta::FromMinutes(40), time_one,
                             time_one, timezone.get(), base::nullopt);

  State expected_state_one;
  expected_state_one.is_locked = false;
  expected_state_one.active_policy = ActivePolicies::kOverride;
  expected_state_one.is_time_usage_limit_enabled = true;
  expected_state_one.remaining_usage = base::TimeDelta::FromMinutes(20);
  expected_state_one.next_state_change_time =
      utils::TimeFromString("Mon, 1 Jan 2018 22:30 PST");
  expected_state_one.next_state_active_policy = ActivePolicies::kUsageLimit;

  AssertEqState(expected_state_one, state_one);

  // Check that the override didn't unlock the device when the time usage limit
  // started, and that it will be locked until the time usage limit reset time,
  // and not when the time window limit ends.
  base::Time time_two = utils::TimeFromString("Mon, 1 Jan 2018 22:30 PST");
  State state_two = GetState(policy, base::TimeDelta::FromHours(1), time_two,
                             time_two, timezone.get(), state_one);

  State expected_state_two;
  expected_state_two.is_locked = true;
  expected_state_two.active_policy = ActivePolicies::kUsageLimit;
  expected_state_two.is_time_usage_limit_enabled = true;
  expected_state_two.remaining_usage = base::TimeDelta::FromMinutes(0);
  expected_state_two.time_usage_limit_started = time_two;
  expected_state_two.next_state_change_time =
      utils::TimeFromString("Tue, 2 Jan 2018 8:00 PST");
  expected_state_two.next_state_active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_two.next_unlock_time =
      utils::TimeFromString("Tue, 2 Jan 2018 8:00 PST");

  AssertEqState(expected_state_two, state_two);
}

// Test unlock override on time usage limit.
TEST_F(UsageTimeLimitProcessorTest, GetStateOverrideTimeUsageLimit) {
  std::unique_ptr<icu::TimeZone> timezone(icu::TimeZone::createTimeZone("PST"));

  // Setup policy.
  base::Time last_updated = utils::TimeFromString("1 Jan 2018 8:00 PST");
  std::unique_ptr<base::DictionaryValue> policy =
      utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));
  utils::AddTimeUsageLimit(policy.get(), utils::kSunday,
                           base::TimeDelta::FromMinutes(60), last_updated);

  base::Time time_one = utils::TimeFromString("Sun, 7 Jan 2018 15:00 PST");
  State state_one = GetState(policy, base::TimeDelta::FromMinutes(40), time_one,
                             time_one, timezone.get(), base::nullopt);

  State expected_state_one;
  expected_state_one.is_locked = false;
  expected_state_one.active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_one.is_time_usage_limit_enabled = true;
  expected_state_one.remaining_usage = base::TimeDelta::FromMinutes(20);
  expected_state_one.next_state_change_time =
      utils::TimeFromString("Sun, 7 Jan 2018 15:20 PST");
  expected_state_one.next_state_active_policy = ActivePolicies::kUsageLimit;

  AssertEqState(expected_state_one, state_one);

  base::Time time_two = utils::TimeFromString("Sun, 7 Jan 2018 15:30 PST");
  State state_two = GetState(policy, base::TimeDelta::FromMinutes(60), time_two,
                             time_two, timezone.get(), state_one);

  State expected_state_two;
  expected_state_two.is_locked = true;
  expected_state_two.active_policy = ActivePolicies::kUsageLimit;
  expected_state_two.is_time_usage_limit_enabled = true;
  expected_state_two.remaining_usage = base::TimeDelta::FromMinutes(0);
  expected_state_two.time_usage_limit_started = time_two;
  expected_state_two.next_state_change_time =
      utils::TimeFromString("Mon, 8 Jan 2018 6:00 PST");
  expected_state_two.next_state_active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_two.next_unlock_time =
      utils::TimeFromString("Mon, 8 Jan 2018 6:00 PST");

  AssertEqState(expected_state_two, state_two);

  utils::AddOverride(policy.get(), utils::kUnlock,
                     utils::TimeFromString("Sun, 7 Jan 2018 16:00 PST"));
  base::Time time_three = utils::TimeFromString("Sun, 7 Jan 2018 16:01 PST");
  State state_three =
      GetState(policy, base::TimeDelta::FromMinutes(60), time_three, time_three,
               timezone.get(), state_two);

  State expected_state_three;
  expected_state_three.is_locked = false;
  expected_state_three.active_policy = ActivePolicies::kOverride;
  expected_state_three.is_time_usage_limit_enabled = true;
  expected_state_three.remaining_usage = base::TimeDelta::FromMinutes(0);
  expected_state_three.time_usage_limit_started = time_two;
  // This should be utils::TimeFromString("Sun, 14 Jan 2018 7:00 PST"),
  // crbug/902348:
  expected_state_three.next_state_change_time = base::Time();
  expected_state_three.next_state_active_policy =
      ActivePolicies::kNoActivePolicy;
  // This should be utils::TimeFromString("Sun, 14 Jan 2018 7:00 PST"),
  // crbug/902348:
  expected_state_three.next_unlock_time = base::Time();

  AssertEqState(expected_state_three, state_three);
}

// Test that the override created on the previous day, does not take effect
// after the reset time on the following day.
TEST_F(UsageTimeLimitProcessorTest, GetStateOldLockOverride) {
  std::unique_ptr<icu::TimeZone> timezone(icu::TimeZone::createTimeZone("PST"));

  // Setup policy.
  std::unique_ptr<base::DictionaryValue> policy =
      utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));
  utils::AddOverride(policy.get(), utils::kLock,
                     utils::TimeFromString("Mon, 1 Jan 2018 21:00 PST"));

  // Check that the device is locked because of the override.
  base::Time time_one = utils::TimeFromString("Mon, 1 Jan 2018 21:00 PST");
  State state_one = GetState(policy, base::TimeDelta::FromMinutes(40), time_one,
                             time_one, timezone.get(), base::nullopt);

  State expected_state_one;
  expected_state_one.is_locked = true;
  expected_state_one.active_policy = ActivePolicies::kOverride;
  expected_state_one.is_time_usage_limit_enabled = false;
  expected_state_one.next_state_change_time =
      utils::TimeFromString("Tue, 2 Jan 2018 6:00 PST");
  expected_state_one.next_state_active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_one.next_unlock_time =
      utils::TimeFromString("Tue, 2 Jan 2018 6:00 PST");

  AssertEqState(expected_state_one, state_one);

  // Check that the device is still locked after midnight.
  base::Time time_two = utils::TimeFromString("Tue, 2 Jan 2018 1:00 PST");
  State state_two = GetState(policy, base::TimeDelta::FromMinutes(0), time_two,
                             time_two, timezone.get(), state_one);

  State expected_state_two;
  expected_state_two.is_locked = true;
  expected_state_two.active_policy = ActivePolicies::kOverride;
  expected_state_two.is_time_usage_limit_enabled = false;
  expected_state_two.next_state_change_time =
      utils::TimeFromString("Tue, 2 Jan 2018 6:00 PST");
  expected_state_two.next_state_active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_two.next_unlock_time =
      utils::TimeFromString("Tue, 2 Jan 2018 6:00 PST");

  AssertEqState(expected_state_two, state_two);

  // Check that the device is unlocked.
  base::Time time_three = utils::TimeFromString("Tue, 2 Jan 2018 6:00 PST");
  State state_three =
      GetState(policy, base::TimeDelta::FromMinutes(0), time_three, time_three,
               timezone.get(), state_two);

  State expected_state_three;
  expected_state_three.is_locked = false;
  expected_state_three.active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_three.is_time_usage_limit_enabled = false;
  expected_state_three.next_state_change_time = base::Time();
  expected_state_three.next_state_active_policy =
      ActivePolicies::kNoActivePolicy;
  expected_state_three.next_unlock_time = base::Time();

  AssertEqState(expected_state_three, state_three);
}

// Make sure that the default time window limit is correctly processed.
TEST_F(UsageTimeLimitProcessorTest, GetStateDefaultBedtime) {
  std::unique_ptr<icu::TimeZone> timezone(icu::TimeZone::createTimeZone("PST"));

  // Setup policy.
  base::Time last_updated = utils::TimeFromString("1 Jan 2018 8:00 PST");
  std::unique_ptr<base::DictionaryValue> policy =
      utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));
  utils::AddTimeWindowLimit(policy.get(), utils::kMonday,
                            utils::CreateTime(21, 0), utils::CreateTime(7, 0),
                            last_updated);
  utils::AddTimeWindowLimit(policy.get(), utils::kTuesday,
                            utils::CreateTime(21, 0), utils::CreateTime(7, 0),
                            last_updated);
  utils::AddTimeWindowLimit(policy.get(), utils::kWednesday,
                            utils::CreateTime(21, 0), utils::CreateTime(7, 0),
                            last_updated);
  utils::AddTimeWindowLimit(policy.get(), utils::kThursday,
                            utils::CreateTime(21, 0), utils::CreateTime(7, 0),
                            last_updated);
  utils::AddTimeWindowLimit(policy.get(), utils::kFriday,
                            utils::CreateTime(21, 0), utils::CreateTime(7, 0),
                            last_updated);
  utils::AddTimeWindowLimit(policy.get(), utils::kSaturday,
                            utils::CreateTime(21, 0), utils::CreateTime(7, 0),
                            last_updated);
  utils::AddTimeWindowLimit(policy.get(), utils::kSunday,
                            utils::CreateTime(21, 0), utils::CreateTime(7, 0),
                            last_updated);

  base::Time monday_ten_pm = utils::TimeFromString("Mon, 1 Jan 2018 22:00 PST");
  base::Time tuesday_five_am =
      utils::TimeFromString("Tue, 2 Jan 2018 5:00 PST");
  base::Time tuesday_seven_am =
      utils::TimeFromString("Tue, 2 Jan 2018 7:00 PST");

  // Test time window limit for every day of the week.
  for (int i = 0; i < 7; i++) {
    // 10 PM on the current day of the week.
    base::Time night_time = monday_ten_pm + base::TimeDelta::FromDays(i);
    // 5 AM on the current day of the week.
    base::Time morning_time = tuesday_five_am + base::TimeDelta::FromDays(i);
    // 7 AM on the current day of the week.
    base::Time window_limit_end_time =
        tuesday_seven_am + base::TimeDelta::FromDays(i);

    State night_state =
        GetState(policy, base::TimeDelta::FromMinutes(40), night_time,
                 night_time, timezone.get(), base::nullopt);

    State expected_night_state;
    expected_night_state.is_locked = true;
    expected_night_state.active_policy = ActivePolicies::kFixedLimit;
    expected_night_state.is_time_usage_limit_enabled = false;
    expected_night_state.remaining_usage = base::TimeDelta::FromMinutes(0);
    expected_night_state.next_state_change_time = window_limit_end_time;
    expected_night_state.next_state_active_policy =
        ActivePolicies::kNoActivePolicy;
    expected_night_state.next_unlock_time = window_limit_end_time;

    AssertEqState(expected_night_state, night_state);

    State morning_state =
        GetState(policy, base::TimeDelta::FromMinutes(40), morning_time,
                 night_time, timezone.get(), base::nullopt);

    State expected_morning_state;
    expected_morning_state.is_locked = true;
    expected_morning_state.active_policy = ActivePolicies::kFixedLimit;
    expected_morning_state.is_time_usage_limit_enabled = false;
    expected_morning_state.remaining_usage = base::TimeDelta::FromMinutes(0);
    expected_morning_state.next_state_change_time = window_limit_end_time;
    expected_morning_state.next_state_active_policy =
        ActivePolicies::kNoActivePolicy;
    expected_morning_state.next_unlock_time = window_limit_end_time;

    AssertEqState(expected_morning_state, morning_state);
  }
}

// Make sure that the default time usage limit is correctly processed.
TEST_F(UsageTimeLimitProcessorTest, GetStateDefaultDailyLimit) {
  std::unique_ptr<icu::TimeZone> timezone(icu::TimeZone::createTimeZone("PST"));

  // Setup policy.
  base::Time last_updated = utils::TimeFromString("1 Jan 2018 8:00 PST");
  std::unique_ptr<base::DictionaryValue> policy =
      utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));
  utils::AddTimeUsageLimit(policy.get(), utils::kMonday,
                           base::TimeDelta::FromHours(3), last_updated);
  utils::AddTimeUsageLimit(policy.get(), utils::kTuesday,
                           base::TimeDelta::FromHours(3), last_updated);
  utils::AddTimeUsageLimit(policy.get(), utils::kWednesday,
                           base::TimeDelta::FromHours(3), last_updated);
  utils::AddTimeUsageLimit(policy.get(), utils::kThursday,
                           base::TimeDelta::FromHours(3), last_updated);
  utils::AddTimeUsageLimit(policy.get(), utils::kFriday,
                           base::TimeDelta::FromHours(3), last_updated);
  utils::AddTimeUsageLimit(policy.get(), utils::kSaturday,
                           base::TimeDelta::FromHours(3), last_updated);
  utils::AddTimeUsageLimit(policy.get(), utils::kSunday,
                           base::TimeDelta::FromHours(3), last_updated);

  base::Time monday_ten_pm = utils::TimeFromString("Mon, 1 Jan 2018 22:00 PST");
  base::Time tuesday_five_am =
      utils::TimeFromString("Tue, 2 Jan 2018 5:00 PST");
  base::Time tuesday_six_am = utils::TimeFromString("Tue, 2 Jan 2018 6:00 PST");

  // Test time usage limit for every day of the week.
  for (int i = 0; i < 7; i++) {
    // 10 PM on the current day of the week.
    base::Time night_time = monday_ten_pm + base::TimeDelta::FromDays(i);
    // 5 AM on the current day of the week.
    base::Time morning_time = tuesday_five_am + base::TimeDelta::FromDays(i);
    // 7 AM on the current day of the week.
    base::Time usage_limit_reset_time =
        tuesday_six_am + base::TimeDelta::FromDays(i);

    State night_state =
        GetState(policy, base::TimeDelta::FromHours(3), night_time, night_time,
                 timezone.get(), base::nullopt);

    State expected_night_state;
    expected_night_state.is_locked = true;
    expected_night_state.active_policy = ActivePolicies::kUsageLimit;
    expected_night_state.is_time_usage_limit_enabled = true;
    expected_night_state.remaining_usage = base::TimeDelta::FromMinutes(0);
    expected_night_state.next_state_change_time = usage_limit_reset_time;
    expected_night_state.next_state_active_policy =
        ActivePolicies::kNoActivePolicy;
    expected_night_state.next_unlock_time = usage_limit_reset_time;
    expected_night_state.time_usage_limit_started = night_time;

    AssertEqState(expected_night_state, night_state);

    State morning_state =
        GetState(policy, base::TimeDelta::FromHours(3), morning_time,
                 night_time, timezone.get(), night_state);

    State expected_morning_state;
    expected_morning_state.is_locked = true;
    expected_morning_state.active_policy = ActivePolicies::kUsageLimit;
    expected_morning_state.is_time_usage_limit_enabled = true;
    expected_morning_state.remaining_usage = base::TimeDelta::FromMinutes(0);
    expected_morning_state.next_state_change_time = usage_limit_reset_time;
    expected_morning_state.next_state_active_policy =
        ActivePolicies::kNoActivePolicy;
    expected_morning_state.next_unlock_time = usage_limit_reset_time;
    expected_morning_state.time_usage_limit_started = night_time;

    AssertEqState(expected_morning_state, morning_state);
  }
}

// Test if an overnight time window limit applies to the following day.
TEST_F(UsageTimeLimitProcessorTest, GetStateWithPreviousDayTimeWindowLimit) {
  std::unique_ptr<icu::TimeZone> timezone(icu::TimeZone::createTimeZone("GMT"));

  // Setup policy.
  base::Time last_updated = utils::TimeFromString("1 Jan 2018 8:00 GMT");
  std::unique_ptr<base::DictionaryValue> policy =
      utils::CreateTimeLimitPolicy(utils::CreateTime(8, 0));
  utils::AddTimeWindowLimit(policy.get(), utils::kSaturday,
                            utils::CreateTime(21, 0), utils::CreateTime(8, 30),
                            last_updated);

  // Check that device is locked.
  base::Time time_one = utils::TimeFromString("Sun, 7 Jan 2018 8:00 GMT");
  State state_one = GetState(policy, base::TimeDelta::FromMinutes(80), time_one,
                             time_one, timezone.get(), base::nullopt);

  State expected_state_one;
  expected_state_one.is_locked = true;
  expected_state_one.active_policy = ActivePolicies::kFixedLimit;
  expected_state_one.is_time_usage_limit_enabled = false;
  expected_state_one.next_state_change_time =
      utils::TimeFromString("Sun, 7 Jan 2018 8:30 GMT");
  expected_state_one.next_state_active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_one.next_unlock_time =
      utils::TimeFromString("Sun, 7 Jan 2018 8:30 GMT");

  AssertEqState(expected_state_one, state_one);
}

// Test if a time usage limit applies to the morning of the following day.
TEST_F(UsageTimeLimitProcessorTest, GetStateWithPreviousDayTimeUsageLimit) {
  std::unique_ptr<icu::TimeZone> timezone(icu::TimeZone::createTimeZone("GMT"));

  // Setup policy.
  base::Time last_updated = utils::TimeFromString("1 Jan 2018 8:00 GMT");
  std::unique_ptr<base::DictionaryValue> policy =
      utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));
  utils::AddTimeUsageLimit(policy.get(), utils::kSaturday,
                           base::TimeDelta::FromHours(2), last_updated);

  // Check that device is locked.
  base::Time time_one = utils::TimeFromString("Sun, 7 Jan 2018 4:00 GMT");
  State state_one = GetState(policy, base::TimeDelta::FromHours(2), time_one,
                             time_one, timezone.get(), base::nullopt);

  State expected_state_one;
  expected_state_one.is_locked = true;
  expected_state_one.active_policy = ActivePolicies::kUsageLimit;
  expected_state_one.is_time_usage_limit_enabled = true;
  expected_state_one.remaining_usage = base::TimeDelta::FromMinutes(0);
  expected_state_one.time_usage_limit_started = time_one;
  expected_state_one.next_state_change_time =
      utils::TimeFromString("Sun, 7 Jan 2018 6:00 GMT");
  expected_state_one.next_state_active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_one.next_unlock_time =
      utils::TimeFromString("Sun, 7 Jan 2018 6:00 GMT");

  AssertEqState(expected_state_one, state_one);
}

// Test if a time usage limit applies to the current night.
TEST_F(UsageTimeLimitProcessorTest, GetStateWithWeekendTimeUsageLimit) {
  std::unique_ptr<icu::TimeZone> timezone(icu::TimeZone::createTimeZone("PST"));

  // Setup policy.
  base::Time last_updated = utils::TimeFromString("1 Jan 2018 8:00 PST");
  std::unique_ptr<base::DictionaryValue> policy =
      utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));
  utils::AddTimeUsageLimit(policy.get(), utils::kSaturday,
                           base::TimeDelta::FromHours(2), last_updated);

  // Check that device is locked.
  base::Time time_one = utils::TimeFromString("Sat, 6 Jan 2018 20:00 PST");
  State state_one = GetState(policy, base::TimeDelta::FromHours(2), time_one,
                             time_one, timezone.get(), base::nullopt);

  State expected_state_one;
  expected_state_one.is_locked = true;
  expected_state_one.active_policy = ActivePolicies::kUsageLimit;
  expected_state_one.is_time_usage_limit_enabled = true;
  expected_state_one.remaining_usage = base::TimeDelta::FromMinutes(0);
  expected_state_one.time_usage_limit_started = time_one;
  expected_state_one.next_state_change_time =
      utils::TimeFromString("Sun, 7 Jan 2018 6:00 PST");
  expected_state_one.next_state_active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_one.next_unlock_time =
      utils::TimeFromString("Sun, 7 Jan 2018 6:00 PST");

  AssertEqState(expected_state_one, state_one);
}

// Test that a lock override followed by a time window limit will be ignored
// after the time window limit is over.
TEST_F(UsageTimeLimitProcessorTest, GetStateLockOverrideFollowedByBedtime) {
  std::unique_ptr<icu::TimeZone> timezone(icu::TimeZone::createTimeZone("PST"));

  // Setup policy.
  base::Time last_updated = utils::TimeFromString("1 Jan 2018 8:00 PST");
  std::unique_ptr<base::DictionaryValue> policy =
      utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));
  utils::AddTimeWindowLimit(policy.get(), utils::kMonday,
                            utils::CreateTime(18, 0), utils::CreateTime(20, 0),
                            last_updated);
  utils::AddOverride(policy.get(), utils::kLock,
                     utils::TimeFromString("Mon, 1 Jan 2018 15:00 PST"));

  // Check that the device is locked because of the override.
  base::Time time_one = utils::TimeFromString("Mon, 1 Jan 2018 15:00 PST");
  State state_one = GetState(policy, base::TimeDelta::FromMinutes(60), time_one,
                             time_one, timezone.get(), base::nullopt);

  State expected_state_one;
  expected_state_one.is_locked = true;
  expected_state_one.active_policy = ActivePolicies::kOverride;
  expected_state_one.is_time_usage_limit_enabled = false;
  expected_state_one.next_state_change_time =
      utils::TimeFromString("Mon, 1 Jan 2018 18:00 PST");
  expected_state_one.next_state_active_policy = ActivePolicies::kFixedLimit;
  expected_state_one.next_unlock_time =
      utils::TimeFromString("Mon, 1 Jan 2018 20:00 PST");

  AssertEqState(expected_state_one, state_one);

  // Check that the device is locked because of the bedtime.
  base::Time time_two = utils::TimeFromString("Mon, 1 Jan 2018 18:00 PST");
  State state_two = GetState(policy, base::TimeDelta::FromMinutes(60), time_two,
                             time_two, timezone.get(), state_one);

  State expected_state_two;
  expected_state_two.is_locked = true;
  expected_state_two.active_policy = ActivePolicies::kFixedLimit;
  expected_state_two.is_time_usage_limit_enabled = false;
  expected_state_two.next_state_change_time =
      utils::TimeFromString("Mon, 1 Jan 2018 20:00 PST");
  expected_state_two.next_state_active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_two.next_unlock_time =
      utils::TimeFromString("Mon, 1 Jan 2018 20:00 PST");

  AssertEqState(expected_state_two, state_two);

  // Check that the device is unlocked after the bedtime ends.
  base::Time time_three = utils::TimeFromString("Mon, 1 Jan 2018 20:00 PST");
  State state_three =
      GetState(policy, base::TimeDelta::FromMinutes(60), time_three, time_three,
               timezone.get(), state_two);

  State expected_state_three;
  expected_state_three.is_locked = false;
  expected_state_three.active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_three.is_time_usage_limit_enabled = false;
  expected_state_three.next_state_change_time =
      utils::TimeFromString("Mon, 8 Jan 2018 18:00 PST");
  expected_state_three.next_state_active_policy = ActivePolicies::kFixedLimit;

  AssertEqState(expected_state_three, state_three);
}

// Test unlock-lock pair during time window limit.
TEST_F(UsageTimeLimitProcessorTest, GetStateUnlockLockDuringBedtime) {
  std::unique_ptr<icu::TimeZone> timezone(icu::TimeZone::createTimeZone("PST"));

  // Setup policy.
  base::Time last_updated = utils::TimeFromString("1 Jan 2018 8:00 PST");
  std::unique_ptr<base::DictionaryValue> policy =
      utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));
  utils::AddTimeWindowLimit(policy.get(), utils::kMonday,
                            utils::CreateTime(10, 0), utils::CreateTime(20, 0),
                            last_updated);
  utils::AddOverride(policy.get(), utils::kUnlock,
                     utils::TimeFromString("Mon, 1 Jan 2018 12:00 PST"));

  // Check that the device is unlocked because of the override.
  base::Time time_one = utils::TimeFromString("Mon, 1 Jan 2018 12:00 PST");
  State state_one = GetState(policy, base::TimeDelta::FromMinutes(60), time_one,
                             time_one, timezone.get(), base::nullopt);

  State expected_state_one;
  expected_state_one.is_locked = false;
  expected_state_one.active_policy = ActivePolicies::kOverride;
  expected_state_one.is_time_usage_limit_enabled = false;
  expected_state_one.next_state_change_time =
      utils::TimeFromString("Mon, 8 Jan 2018 10:00 PST");
  expected_state_one.next_state_active_policy = ActivePolicies::kFixedLimit;

  AssertEqState(expected_state_one, state_one);

  // Create lock.
  utils::AddOverride(policy.get(), utils::kLock,
                     utils::TimeFromString("Mon, 1 Jan 2018 14:00 PST"));

  // Check that the device is locked because of the bedtime.
  base::Time time_two = utils::TimeFromString("Mon, 1 Jan 2018 14:00 PST");
  State state_two = GetState(policy, base::TimeDelta::FromMinutes(60), time_two,
                             time_two, timezone.get(), state_one);

  State expected_state_two;
  expected_state_two.is_locked = true;
  expected_state_two.active_policy = ActivePolicies::kFixedLimit;
  expected_state_two.is_time_usage_limit_enabled = false;
  expected_state_two.next_state_change_time =
      utils::TimeFromString("Mon, 1 Jan 2018 20:00 PST");
  expected_state_two.next_state_active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_two.next_unlock_time =
      utils::TimeFromString("Mon, 1 Jan 2018 20:00 PST");

  AssertEqState(expected_state_two, state_two);

  // Check that the device is unlocked after the bedtime ends.
  base::Time time_three = utils::TimeFromString("Mon, 1 Jan 2018 20:00 PST");
  State state_three =
      GetState(policy, base::TimeDelta::FromMinutes(60), time_three, time_three,
               timezone.get(), state_two);

  State expected_state_three;
  expected_state_three.is_locked = false;
  expected_state_three.active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_three.is_time_usage_limit_enabled = false;
  expected_state_three.next_state_change_time =
      utils::TimeFromString("Mon, 8 Jan 2018 10:00 PST");
  expected_state_three.next_state_active_policy = ActivePolicies::kFixedLimit;

  AssertEqState(expected_state_three, state_three);
}

// Test if increasing the usage limit unlocks the device that was previously
// locked by it.
TEST_F(UsageTimeLimitProcessorTest, GetStateIncreaseUsageLimitAfterLocked) {
  std::unique_ptr<icu::TimeZone> timezone(icu::TimeZone::createTimeZone("BRT"));

  // Setup policy.
  base::Time last_updated = utils::TimeFromString("1 Jan 2018 8:00 BRT");
  std::unique_ptr<base::DictionaryValue> policy =
      utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));
  utils::AddTimeUsageLimit(policy.get(), utils::kWednesday,
                           base::TimeDelta::FromHours(2), last_updated);

  // Check that the device is unlocked because of the override.
  base::Time time_one = utils::TimeFromString("Wed, 3 Jan 2018 14:00 BRT");
  State state_one = GetState(policy, base::TimeDelta::FromHours(2), time_one,
                             time_one, timezone.get(), base::nullopt);

  State expected_state_one;
  expected_state_one.is_locked = true;
  expected_state_one.active_policy = ActivePolicies::kUsageLimit;
  expected_state_one.is_time_usage_limit_enabled = true;
  expected_state_one.remaining_usage = base::TimeDelta::FromMinutes(0);
  expected_state_one.time_usage_limit_started = time_one;
  expected_state_one.next_state_change_time =
      utils::TimeFromString("Thu, 4 Jan 2018 6:00 BRT");
  expected_state_one.next_state_active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_one.next_unlock_time =
      utils::TimeFromString("Thu, 4 Jan 2018 6:00 BRT");

  AssertEqState(expected_state_one, state_one);

  // Create unlock.
  utils::AddOverride(policy.get(), utils::kUnlock,
                     utils::TimeFromString("Wed, 3 Jan 2018 15:00 BRT"));

  // Check that the device is unlocked.
  base::Time time_two = utils::TimeFromString("Wed, 3 Jan 2018 15:00 BRT");
  State state_two = GetState(policy, base::TimeDelta::FromHours(2), time_two,
                             time_two, timezone.get(), state_one);

  State expected_state_two;
  expected_state_two.is_locked = false;
  expected_state_two.active_policy = ActivePolicies::kOverride;
  expected_state_two.is_time_usage_limit_enabled = true;
  expected_state_two.remaining_usage = base::TimeDelta::FromMinutes(0);
  expected_state_two.time_usage_limit_started = time_one;
  expected_state_two.next_state_change_time = base::Time();
  expected_state_two.next_state_active_policy = ActivePolicies::kNoActivePolicy;

  AssertEqState(expected_state_two, state_two);

  // Create lock.
  utils::AddOverride(policy.get(), utils::kLock,
                     utils::TimeFromString("Wed, 3 Jan 2018 16:00 BRT"));

  // Check that the device is locked because of the usage limit.
  base::Time time_three = utils::TimeFromString("Wed, 3 Jan 2018 16:00 BRT");
  State state_three =
      GetState(policy, base::TimeDelta::FromHours(2), time_three, time_three,
               timezone.get(), state_two);

  State expected_state_three;
  expected_state_three.is_locked = true;
  expected_state_three.active_policy = ActivePolicies::kUsageLimit;
  expected_state_three.is_time_usage_limit_enabled = true;
  expected_state_three.remaining_usage = base::TimeDelta::FromMinutes(0);
  expected_state_three.time_usage_limit_started = time_one;
  expected_state_three.next_state_change_time =
      utils::TimeFromString("Thu, 4 Jan 2018 6:00 BRT");
  expected_state_three.next_state_active_policy =
      ActivePolicies::kNoActivePolicy;
  expected_state_three.next_unlock_time =
      utils::TimeFromString("Thu, 4 Jan 2018 6:00 BRT");

  AssertEqState(expected_state_three, state_three);

  // Update usage time limit.
  utils::AddTimeUsageLimit(policy.get(), utils::kWednesday,
                           base::TimeDelta::FromHours(3),
                           utils::TimeFromString("3 Jan 2018 17:00 BRT"));

  // Check that the device is locked because of the bedtime.
  base::Time time_four = utils::TimeFromString("Wed, 3 Jan 2018 17:00 BRT");
  State state_four = GetState(policy, base::TimeDelta::FromHours(2), time_four,
                              time_four, timezone.get(), state_two);

  State expected_state_four;
  expected_state_four.is_locked = false;
  expected_state_four.active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_four.is_time_usage_limit_enabled = true;
  expected_state_four.remaining_usage = base::TimeDelta::FromMinutes(60);
  expected_state_four.time_usage_limit_started = base::Time();
  expected_state_four.next_state_change_time =
      utils::TimeFromString("Wed, 3 Jan 2018 18:00 BRT");
  expected_state_four.next_state_active_policy = ActivePolicies::kUsageLimit;

  AssertEqState(expected_state_four, state_four);
}

// Test if consecutive locked all day usage limits work properly.
TEST_F(UsageTimeLimitProcessorTest,
       GetStateConsecutiveLockedAllDayAfterUnlock) {
  std::unique_ptr<icu::TimeZone> timezone(icu::TimeZone::createTimeZone("BRT"));

  // Setup policy.
  base::Time last_updated = utils::TimeFromString("1 Jan 2018 8:00 BRT");
  std::unique_ptr<base::DictionaryValue> policy =
      utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));
  utils::AddTimeUsageLimit(policy.get(), utils::kWednesday,
                           base::TimeDelta::FromHours(0), last_updated);
  utils::AddTimeUsageLimit(policy.get(), utils::kThursday,
                           base::TimeDelta::FromHours(0), last_updated);
  utils::AddTimeUsageLimit(policy.get(), utils::kFriday,
                           base::TimeDelta::FromHours(0), last_updated);

  // Check that the device is locked.
  base::Time time_one = utils::TimeFromString("Wed, 3 Jan 2018 7:00 BRT");
  State state_one = GetState(policy, base::TimeDelta::FromHours(0), time_one,
                             time_one, timezone.get(), base::nullopt);

  State expected_state_one;
  expected_state_one.is_locked = true;
  expected_state_one.active_policy = ActivePolicies::kUsageLimit;
  expected_state_one.is_time_usage_limit_enabled = true;
  expected_state_one.remaining_usage = base::TimeDelta::FromMinutes(0);
  expected_state_one.time_usage_limit_started = time_one;
  expected_state_one.next_state_change_time =
      utils::TimeFromString("Thu, 4 Jan 2018 6:00 BRT");
  expected_state_one.next_state_active_policy = ActivePolicies::kUsageLimit;
  expected_state_one.next_unlock_time =
      utils::TimeFromString("Thu, 4 Jan 2018 6:00 BRT");

  AssertEqState(expected_state_one, state_one);

  utils::AddOverride(policy.get(), utils::kUnlock,
                     utils::TimeFromString("Wed, 3 Jan 2018 7:30 BRT"));

  // Check that the device is unlocked because of the override.
  base::Time time_two = utils::TimeFromString("Wed, 3 Jan 2018 8:00 BRT");
  State state_two = GetState(policy, base::TimeDelta::FromHours(0), time_two,
                             time_two, timezone.get(), state_one);

  State expected_state_two;
  expected_state_two.is_locked = false;
  expected_state_two.active_policy = ActivePolicies::kOverride;
  expected_state_two.is_time_usage_limit_enabled = true;
  expected_state_two.remaining_usage = base::TimeDelta::FromMinutes(0);
  expected_state_two.time_usage_limit_started = time_one;
  expected_state_two.next_state_change_time =
      utils::TimeFromString("Thu, 4 Jan 2018 6:00 BRT");
  expected_state_two.next_state_active_policy = ActivePolicies::kUsageLimit;

  AssertEqState(expected_state_two, state_two);

  // Check that the device is locked.
  base::Time time_three = utils::TimeFromString("Thu, 4 Jan 2018 8:00 BRT");
  State state_three =
      GetState(policy, base::TimeDelta::FromHours(0), time_three, time_three,
               timezone.get(), state_two);

  State expected_state_three;
  expected_state_three.is_locked = true;
  expected_state_three.active_policy = ActivePolicies::kUsageLimit;
  expected_state_three.is_time_usage_limit_enabled = true;
  expected_state_three.remaining_usage = base::TimeDelta::FromMinutes(0);
  expected_state_three.time_usage_limit_started = time_one;
  expected_state_three.next_state_change_time =
      utils::TimeFromString("Fri, 5 Jan 2018 6:00 BRT");
  expected_state_three.next_state_active_policy = ActivePolicies::kUsageLimit;
  expected_state_three.next_unlock_time =
      utils::TimeFromString("Fri, 5 Jan 2018 6:00 BRT");

  AssertEqState(expected_state_three, state_three);

  // Check that the device is locked.
  base::Time time_four = utils::TimeFromString("Fri, 5 Jan 2018 8:00 BRT");
  State state_four = GetState(policy, base::TimeDelta::FromHours(0), time_four,
                              time_four, timezone.get(), state_three);

  State expected_state_four;
  expected_state_four.is_locked = true;
  expected_state_four.active_policy = ActivePolicies::kUsageLimit;
  expected_state_four.is_time_usage_limit_enabled = true;
  expected_state_four.remaining_usage = base::TimeDelta::FromMinutes(0);
  expected_state_four.time_usage_limit_started = time_one;
  expected_state_four.next_state_change_time =
      utils::TimeFromString("Sat, 6 Jan 2018 6:00 BRT");
  expected_state_four.next_state_active_policy =
      ActivePolicies::kNoActivePolicy;
  expected_state_four.next_unlock_time =
      utils::TimeFromString("Sat, 6 Jan 2018 6:00 BRT");

  AssertEqState(expected_state_four, state_four);

  // Check that the device is unlocked.
  base::Time time_five = utils::TimeFromString("Sat, 6 Jan 2018 6:00 BRT");
  State state_five = GetState(policy, base::TimeDelta::FromHours(0), time_five,
                              time_five, timezone.get(), state_four);

  State expected_state_five;
  expected_state_five.is_locked = false;
  expected_state_five.active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_five.is_time_usage_limit_enabled = false;
  expected_state_five.next_state_change_time =
      utils::TimeFromString("Wed, 10 Jan 2018 6:00 BRT");
  expected_state_five.next_state_active_policy = ActivePolicies::kUsageLimit;

  AssertEqState(expected_state_five, state_five);
}

// Test if unlock work after two consecutive daily limit.
TEST_F(UsageTimeLimitProcessorTest, GetStateUnlockConsecutiveLockedAllDay) {
  std::unique_ptr<icu::TimeZone> timezone(icu::TimeZone::createTimeZone("BRT"));

  // Setup policy.
  base::Time last_updated = utils::TimeFromString("1 Jan 2018 8:00 BRT");
  std::unique_ptr<base::DictionaryValue> policy =
      utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));
  utils::AddTimeUsageLimit(policy.get(), utils::kWednesday,
                           base::TimeDelta::FromHours(0), last_updated);
  utils::AddTimeUsageLimit(policy.get(), utils::kThursday,
                           base::TimeDelta::FromHours(0), last_updated);

  // Check that the device is locked.
  base::Time time_one = utils::TimeFromString("Wed, 3 Jan 2018 7:00 BRT");
  State state_one = GetState(policy, base::TimeDelta::FromHours(0), time_one,
                             time_one, timezone.get(), base::nullopt);

  State expected_state_one;
  expected_state_one.is_locked = true;
  expected_state_one.active_policy = ActivePolicies::kUsageLimit;
  expected_state_one.is_time_usage_limit_enabled = true;
  expected_state_one.remaining_usage = base::TimeDelta::FromMinutes(0);
  expected_state_one.time_usage_limit_started = time_one;
  expected_state_one.next_state_change_time =
      utils::TimeFromString("Thu, 4 Jan 2018 6:00 BRT");
  expected_state_one.next_state_active_policy = ActivePolicies::kUsageLimit;
  expected_state_one.next_unlock_time =
      utils::TimeFromString("Thu, 4 Jan 2018 6:00 BRT");

  AssertEqState(expected_state_one, state_one);

  // Check that the device is locked.
  base::Time time_two = utils::TimeFromString("Thu, 4 Jan 2018 6:00 BRT");
  State state_two = GetState(policy, base::TimeDelta::FromHours(0), time_two,
                             time_two, timezone.get(), state_one);

  State expected_state_two;
  expected_state_two.is_locked = true;
  expected_state_two.active_policy = ActivePolicies::kUsageLimit;
  expected_state_two.is_time_usage_limit_enabled = true;
  expected_state_two.remaining_usage = base::TimeDelta::FromMinutes(0);
  expected_state_two.time_usage_limit_started = time_one;
  expected_state_two.next_state_change_time =
      utils::TimeFromString("Fri, 5 Jan 2018 6:00 BRT");
  expected_state_two.next_state_active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_two.next_unlock_time =
      utils::TimeFromString("Fri, 5 Jan 2018 6:00 BRT");

  AssertEqState(expected_state_two, state_two);

  utils::AddOverride(policy.get(), utils::kUnlock,
                     utils::TimeFromString("Thu, 4 Jan 2018 7:30 BRT"));

  // Check that the device is unlocked.
  base::Time time_three = utils::TimeFromString("Thu, 4 Jan 2018 8:00 BRT");
  State state_three =
      GetState(policy, base::TimeDelta::FromHours(0), time_three, time_three,
               timezone.get(), state_two);

  State expected_state_three;
  expected_state_three.is_locked = false;
  expected_state_three.active_policy = ActivePolicies::kOverride;
  expected_state_three.is_time_usage_limit_enabled = true;
  expected_state_three.remaining_usage = base::TimeDelta::FromMinutes(0);
  expected_state_three.time_usage_limit_started = time_one;
  expected_state_three.next_state_change_time =
      utils::TimeFromString("Wed, 10 Jan 2018 6:00 BRT");
  expected_state_three.next_state_active_policy = ActivePolicies::kUsageLimit;

  AssertEqState(expected_state_three, state_three);
}

// Test GetExpectedResetTime with an empty policy.
TEST_F(UsageTimeLimitProcessorTest, GetExpectedResetTimeWithEmptyPolicy) {
  std::unique_ptr<icu::TimeZone> timezone(icu::TimeZone::createTimeZone("GMT"));

  // Setup policy.
  std::unique_ptr<base::DictionaryValue> policy =
      std::make_unique<base::DictionaryValue>(base::DictionaryValue());

  base::Time time_one = utils::TimeFromString("Mon, 1 Jan 2018 22:00");
  base::Time reset_time =
      GetExpectedResetTime(policy, time_one, timezone.get());

  ASSERT_EQ(reset_time, utils::TimeFromString("Tue, 2 Jan 2018 0:00"));
}

// Test GetExpectedResetTime with a custom time usage limit reset time.
TEST_F(UsageTimeLimitProcessorTest, GetExpectedResetTimeWithCustomPolicy) {
  std::unique_ptr<icu::TimeZone> timezone(icu::TimeZone::createTimeZone("EST"));

  // Setup policy.
  std::unique_ptr<base::DictionaryValue> policy =
      utils::CreateTimeLimitPolicy(utils::CreateTime(8, 0));

  // Check that it resets in the same day.
  base::Time time_one = utils::TimeFromString("Tue, 2 Jan 2018 6:00 EST");
  base::Time reset_time_one =
      GetExpectedResetTime(policy, time_one, timezone.get());

  ASSERT_EQ(reset_time_one, utils::TimeFromString("Tue, 2 Jan 2018 8:00 EST"));

  // Checks that it resets on the following day.
  base::Time time_two = utils::TimeFromString("Tue, 2 Jan 2018 10:00 EST");
  base::Time reset_time_two =
      GetExpectedResetTime(policy, time_two, timezone.get());

  ASSERT_EQ(reset_time_two, utils::TimeFromString("Wed, 3 Jan 2018 8:00 EST"));
}

TEST_F(UsageTimeLimitProcessorTest, GetTimeUsageLimitResetTime) {
  // If there is no valid time usage limit in the policy, default value
  // (midnight) should be returned.
  auto empty_time_limit =
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
  auto empty_time_limit_dictionary =
      base::DictionaryValue::From(std::move(empty_time_limit));

  EXPECT_EQ(base::TimeDelta::FromHours(0),
            GetTimeUsageLimitResetTime(empty_time_limit_dictionary));

  // If reset time is specified in the time usage limit policy, its value should
  // be returned.
  const int kHour = 8;
  const int kMinutes = 30;
  auto time_usage_limit = base::Value(base::Value::Type::DICTIONARY);
  time_usage_limit.SetKey(
      "reset_at", utils::CreatePolicyTime(utils::CreateTime(kHour, kMinutes)));
  auto time_limit =
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
  time_limit->SetKey("time_usage_limit", std::move(time_usage_limit));
  auto time_limit_dictionary =
      base::DictionaryValue::From(std::move(time_limit));

  EXPECT_EQ(base::TimeDelta::FromHours(kHour) +
                base::TimeDelta::FromMinutes(kMinutes),
            GetTimeUsageLimitResetTime(time_limit_dictionary));
}

}  // namespace usage_time_limit
}  // namespace chromeos
