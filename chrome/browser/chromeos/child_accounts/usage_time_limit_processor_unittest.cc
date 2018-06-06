// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "usage_time_limit_processor.h"

#include <memory>

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace internal {

class UsageTimeLimitProcessorTest : public testing::Test {
 public:
  UsageTimeLimitProcessorTest() {}
  ~UsageTimeLimitProcessorTest() override = default;

  base::Value CreateTime(int hour, int minute) {
    base::Value time(base::Value::Type::DICTIONARY);
    time.SetKey("hour", base::Value(hour));
    time.SetKey("minute", base::Value(minute));
    return time;
  }

  base::Value CreateTimeWindow(base::Value day,
                               base::Value start,
                               base::Value end,
                               base::Value last_updated) {
    base::Value time_window(base::Value::Type::DICTIONARY);
    time_window.SetKey("effective_day", std::move(day));
    time_window.SetKey("starts_at", std::move(start));
    time_window.SetKey("ends_at", std::move(end));
    time_window.SetKey("last_updated_millis", std::move(last_updated));
    return time_window;
  }

  base::Value CreateTimeUsage(base::Value usage_quota,
                              base::Value last_updated) {
    base::Value time_usage(base::Value::Type::DICTIONARY);
    time_usage.SetKey("usage_quota_mins", std::move(usage_quota));
    time_usage.SetKey("last_updated_millis", std::move(last_updated));
    return time_usage;
  }

  std::string CreatePolicyTimestamp(const char* time_string) {
    base::Time time;
    if (!base::Time::FromUTCString(time_string, &time)) {
      LOG(ERROR) << "Wrong time string format.";
    }
    return std::to_string(
        base::TimeDelta(time - base::Time::UnixEpoch()).InMilliseconds());
  }
};

// Validates that a well formed dictionary containing the time_window_limit
// information from the UsageTimeLimit policy is converted to its intermediate
// representation correctly.
TEST_F(UsageTimeLimitProcessorTest, TimeLimitWindowValid) {
  // Create dictionary containing the policy information.
  std::string last_updated_millis =
      CreatePolicyTimestamp("1 Jan 1970 00:00:00");
  base::Value monday_time_limit =
      CreateTimeWindow(base::Value("MONDAY"), CreateTime(22, 30),
                       CreateTime(7, 30), base::Value(last_updated_millis));
  base::Value friday_time_limit =
      CreateTimeWindow(base::Value("FRIDAY"), CreateTime(23, 0),
                       CreateTime(8, 20), base::Value(last_updated_millis));

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
TEST_F(UsageTimeLimitProcessorTest, TimeUsageWindowValid) {
  // Create dictionary containing the policy information.
  std::string last_updated_millis_one =
      CreatePolicyTimestamp("1 Jan 2018 10:00:00");
  std::string last_updated_millis_two =
      CreatePolicyTimestamp("1 Jan 2018 11:00:00");
  base::Value tuesday_time_usage =
      CreateTimeUsage(base::Value(120), base::Value(last_updated_millis_one));
  base::Value thursday_time_usage =
      CreateTimeUsage(base::Value(80), base::Value(last_updated_millis_two));

  base::Value time_usage_limit = base::Value(base::Value::Type::DICTIONARY);
  time_usage_limit.SetKey("tuesday", std::move(tuesday_time_usage));
  time_usage_limit.SetKey("thursday", std::move(thursday_time_usage));
  time_usage_limit.SetKey("reset_at", CreateTime(8, 0));

  // Call tested functions.
  TimeUsageLimit usage_limit_struct(time_usage_limit);

  ASSERT_EQ(usage_limit_struct.reset_at.InMinutes(), 8 * 60);

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
TEST_F(UsageTimeLimitProcessorTest, OverrideValid) {
  // Create policy information.
  std::string created_at_millis = CreatePolicyTimestamp("1 Jan 2018 10:00:00");
  base::Value override = base::Value(base::Value::Type::DICTIONARY);
  override.SetKey("action", base::Value("UNLOCK"));
  override.SetKey("created_at_millis", base::Value(created_at_millis));

  // Call tested functions.
  Override override_struct(override);

  // Assert right fields are set.
  ASSERT_EQ(override_struct.action, Override::Action::kUnlock);
  ASSERT_EQ(override_struct.created_at, base::Time::FromDoubleT(1514800800));
  ASSERT_FALSE(override_struct.duration);
}

}  // namespace internal
}  // namespace chromeos
