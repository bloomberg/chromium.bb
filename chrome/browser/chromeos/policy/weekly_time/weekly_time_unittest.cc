// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/weekly_time/weekly_time.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/callback_helpers.h"
#include "base/i18n/rtl.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/icu_test_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace em = enterprise_management;

namespace policy {

namespace {

enum {
  kMonday = 1,
  kTuesday = 2,
  kWednesday = 3,
  kThursday = 4,
  kFriday = 5,
  kSaturday = 6,
  kSunday = 7,
};

constexpr em::WeeklyTimeProto_DayOfWeek kWeekdays[] = {
    em::WeeklyTimeProto::DAY_OF_WEEK_UNSPECIFIED,
    em::WeeklyTimeProto::MONDAY,
    em::WeeklyTimeProto::TUESDAY,
    em::WeeklyTimeProto::WEDNESDAY,
    em::WeeklyTimeProto::THURSDAY,
    em::WeeklyTimeProto::FRIDAY,
    em::WeeklyTimeProto::SATURDAY,
    em::WeeklyTimeProto::SUNDAY};

const int kMinutesInHour = 60;

constexpr base::TimeDelta kMinute = base::TimeDelta::FromMinutes(1);
constexpr base::TimeDelta kHour = base::TimeDelta::FromHours(1);
constexpr base::TimeDelta kWeek = base::TimeDelta::FromDays(7);

}  // namespace

class SingleWeeklyTimeTest
    : public testing::TestWithParam<std::tuple<int, int>> {
 public:
  int day_of_week() const { return std::get<0>(GetParam()); }
  int minutes() const { return std::get<1>(GetParam()); }
};

TEST_P(SingleWeeklyTimeTest, Constructor) {
  WeeklyTime weekly_time =
      WeeklyTime(day_of_week(), minutes() * kMinute.InMilliseconds());
  EXPECT_EQ(weekly_time.day_of_week(), day_of_week());
  EXPECT_EQ(weekly_time.milliseconds(), minutes() * kMinute.InMilliseconds());
}

TEST_P(SingleWeeklyTimeTest, ToValue) {
  WeeklyTime weekly_time =
      WeeklyTime(day_of_week(), minutes() * kMinute.InMilliseconds());
  std::unique_ptr<base::DictionaryValue> weekly_time_value =
      weekly_time.ToValue();
  base::DictionaryValue expected_weekly_time;
  expected_weekly_time.SetInteger("day_of_week", day_of_week());
  expected_weekly_time.SetInteger("time", minutes() * kMinute.InMilliseconds());
  EXPECT_EQ(*weekly_time_value, expected_weekly_time);
}

TEST_P(SingleWeeklyTimeTest, ExtractFromProto_InvalidDay) {
  int milliseconds = minutes() * kMinute.InMilliseconds();
  em::WeeklyTimeProto proto;
  proto.set_day_of_week(kWeekdays[0]);
  proto.set_time(milliseconds);
  auto result = WeeklyTime::ExtractFromProto(proto);
  ASSERT_FALSE(result);
}

TEST_P(SingleWeeklyTimeTest, ExtractFromProto_InvalidTime) {
  em::WeeklyTimeProto proto;
  proto.set_day_of_week(kWeekdays[day_of_week()]);
  proto.set_time(-1);
  auto result = WeeklyTime::ExtractFromProto(proto);
  ASSERT_FALSE(result);
}

TEST_P(SingleWeeklyTimeTest, ExtractFromProto_Valid) {
  int milliseconds = minutes() * kMinute.InMilliseconds();
  em::WeeklyTimeProto proto;
  proto.set_day_of_week(kWeekdays[day_of_week()]);
  proto.set_time(milliseconds);
  auto result = WeeklyTime::ExtractFromProto(proto);
  ASSERT_TRUE(result);
  EXPECT_EQ(result->day_of_week(), day_of_week());
  EXPECT_EQ(result->milliseconds(), milliseconds);
}

INSTANTIATE_TEST_CASE_P(TheSmallestCase,
                        SingleWeeklyTimeTest,
                        testing::Values(std::make_tuple(kMonday, 0)));

INSTANTIATE_TEST_CASE_P(
    TheBiggestCase,
    SingleWeeklyTimeTest,
    testing::Values(std::make_tuple(kSunday, 24 * kMinutesInHour - 1)));

INSTANTIATE_TEST_CASE_P(
    RandomCase,
    SingleWeeklyTimeTest,
    testing::Values(std::make_tuple(kWednesday, 15 * kMinutesInHour + 30)));

class TwoWeeklyTimesAndDurationTest
    : public testing::TestWithParam<
          std::tuple<int, int, int, int, base::TimeDelta>> {
 public:
  int day1() const { return std::get<0>(GetParam()); }
  int minutes1() const { return std::get<1>(GetParam()); }
  int day2() const { return std::get<2>(GetParam()); }
  int minutes2() const { return std::get<3>(GetParam()); }
  base::TimeDelta expected_duration() const { return std::get<4>(GetParam()); }
};

TEST_P(TwoWeeklyTimesAndDurationTest, GetDuration) {
  WeeklyTime weekly_time1 =
      WeeklyTime(day1(), minutes1() * kMinute.InMilliseconds());
  WeeklyTime weekly_time2 =
      WeeklyTime(day2(), minutes2() * kMinute.InMilliseconds());
  EXPECT_EQ(weekly_time1.GetDurationTo(weekly_time2), expected_duration());
}

INSTANTIATE_TEST_CASE_P(ZeroDuration,
                        TwoWeeklyTimesAndDurationTest,
                        testing::Values(std::make_tuple(kWednesday,
                                                        kMinutesInHour,
                                                        kWednesday,
                                                        kMinutesInHour,
                                                        base::TimeDelta())));

INSTANTIATE_TEST_CASE_P(TheLongestDuration,
                        TwoWeeklyTimesAndDurationTest,
                        testing::Values(std::make_tuple(kMonday,
                                                        0,
                                                        kSunday,
                                                        24 * kMinutesInHour - 1,
                                                        kWeek - kMinute)));

INSTANTIATE_TEST_CASE_P(
    DifferentDurations,
    TwoWeeklyTimesAndDurationTest,
    testing::Values(
        std::make_tuple(kThursday, 54, kThursday, kMinutesInHour + 54, kHour),
        std::make_tuple(kSunday, 24 * kMinutesInHour - 1, kMonday, 0, kMinute),
        std::make_tuple(kSaturday,
                        15 * kMinutesInHour + 30,
                        kFriday,
                        17 * kMinutesInHour + 45,
                        base::TimeDelta::FromDays(6) +
                            base::TimeDelta::FromHours(2) +
                            base::TimeDelta::FromMinutes(15))));

class TwoWeeklyTimesAndOffsetTest
    : public testing::TestWithParam<std::tuple<int, int, int, int, int>> {
 public:
  int day_of_week() const { return std::get<0>(GetParam()); }
  int minutes() const { return std::get<1>(GetParam()); }
  int offset_minutes() const { return std::get<2>(GetParam()); }
  int expected_day() const { return std::get<3>(GetParam()); }
  int expected_minutes() const { return std::get<4>(GetParam()); }
};

TEST_P(TwoWeeklyTimesAndOffsetTest, AddMilliseconds) {
  WeeklyTime weekly_time =
      WeeklyTime(day_of_week(), minutes() * kMinute.InMilliseconds());
  WeeklyTime result_weekly_time =
      weekly_time.AddMilliseconds(offset_minutes() * kMinute.InMilliseconds());
  EXPECT_EQ(result_weekly_time.day_of_week(), expected_day());
  EXPECT_EQ(result_weekly_time.milliseconds(),
            expected_minutes() * kMinute.InMilliseconds());
}

INSTANTIATE_TEST_CASE_P(
    ZeroOffset,
    TwoWeeklyTimesAndOffsetTest,
    testing::Values(std::make_tuple(kTuesday,
                                    15 * kMinutesInHour + 30,
                                    0,
                                    kTuesday,
                                    15 * kMinutesInHour + 30)));

INSTANTIATE_TEST_CASE_P(
    TheSmallestOffset,
    TwoWeeklyTimesAndOffsetTest,
    testing::Values(std::make_tuple(kWednesday,
                                    15 * kMinutesInHour + 30,
                                    -13 * kMinutesInHour,
                                    kWednesday,
                                    2 * kMinutesInHour + 30),
                    std::make_tuple(kMonday,
                                    9 * kMinutesInHour + 30,
                                    -13 * kMinutesInHour,
                                    kSunday,
                                    20 * kMinutesInHour + 30)));

INSTANTIATE_TEST_CASE_P(
    TheBiggestOffset,
    TwoWeeklyTimesAndOffsetTest,
    testing::Values(std::make_tuple(kTuesday,
                                    10 * kMinutesInHour + 30,
                                    13 * kMinutesInHour,
                                    kTuesday,
                                    23 * kMinutesInHour + 30),
                    std::make_tuple(kSunday,
                                    21 * kMinutesInHour + 30,
                                    13 * kMinutesInHour,
                                    kMonday,
                                    10 * kMinutesInHour + 30)));

INSTANTIATE_TEST_CASE_P(
    DifferentOffsets,
    TwoWeeklyTimesAndOffsetTest,
    testing::Values(std::make_tuple(kWednesday,
                                    10 * kMinutesInHour + 47,
                                    5 * kMinutesInHour + 30,
                                    kWednesday,
                                    16 * kMinutesInHour + 17),
                    std::make_tuple(kMonday,
                                    10 * kMinutesInHour + 47,
                                    6 * kMinutesInHour + 15,
                                    kMonday,
                                    17 * kMinutesInHour + 2),
                    std::make_tuple(kThursday,
                                    22 * kMinutesInHour + 24,
                                    -7 * kMinutesInHour,
                                    kThursday,
                                    15 * kMinutesInHour + 24)));

TEST(WeeklyTimeStringTest, ToLocalizedString) {
  base::test::ScopedRestoreICUDefaultLocale restore_locale;

  // 15:50 UTC, 8:50 PT, 11:50 PT
  WeeklyTime test_weekly_time =
      WeeklyTime(5, (15 * kMinutesInHour + 50) * kMinute.InMilliseconds());

  // Save original timezone
  base::ScopedClosureRunner reset_timezone(base::BindOnce(
      [](std::unique_ptr<icu::TimeZone> original_timezone) {
        icu::TimeZone::adoptDefault(original_timezone.release());
      },
      base::WrapUnique<icu::TimeZone>(icu::TimeZone::createDefault())));

  base::i18n::SetICUDefaultLocale("en_US");
  icu::TimeZone::adoptDefault(
      icu::TimeZone::createTimeZone("America/Los_Angeles"));
  EXPECT_EQ(base::UTF8ToUTF16("Friday 8:50 AM"),
            test_weekly_time.ToLocalizedString());

  base::i18n::SetICUDefaultLocale("de_DE");
  EXPECT_EQ(base::UTF8ToUTF16("Freitag, 08:50"),
            test_weekly_time.ToLocalizedString());

  base::i18n::SetICUDefaultLocale("en_GB");
  icu::TimeZone::adoptDefault(
      icu::TimeZone::createTimeZone("America/New_York"));
  EXPECT_EQ(base::UTF8ToUTF16("Friday 11:50"),
            test_weekly_time.ToLocalizedString());
}

}  // namespace policy
