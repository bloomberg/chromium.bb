// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <time.h>

#include "base/basictypes.h"
#include "base/string16.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/time_format.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;

TEST(TimeFormat, RelativeDate) {
  Time now = Time::Now();
  string16 today_str = TimeFormat::RelativeDate(now, NULL);
  EXPECT_EQ(ASCIIToUTF16("Today"), today_str);

  Time yesterday = now - TimeDelta::FromDays(1);
  string16 yesterday_str = TimeFormat::RelativeDate(yesterday, NULL);
  EXPECT_EQ(ASCIIToUTF16("Yesterday"), yesterday_str);

  Time two_days_ago = now - TimeDelta::FromDays(2);
  string16 two_days_ago_str = TimeFormat::RelativeDate(two_days_ago, NULL);
  EXPECT_TRUE(two_days_ago_str.empty());

  Time a_week_ago = now - TimeDelta::FromDays(7);
  string16 a_week_ago_str = TimeFormat::RelativeDate(a_week_ago, NULL);
  EXPECT_TRUE(a_week_ago_str.empty());
}

namespace {
void TestTimeFormats(const TimeDelta delta, const char* expected_ascii) {
  string16 expected = ASCIIToUTF16(expected_ascii);
  string16 expected_left = expected + ASCIIToUTF16(" left");
  string16 expected_ago = expected + ASCIIToUTF16(" ago");
  EXPECT_EQ(expected, TimeFormat::TimeRemainingShort(delta));
  EXPECT_EQ(expected_left, TimeFormat::TimeRemaining(delta));
  EXPECT_EQ(expected_ago, TimeFormat::TimeElapsed(delta));
}

} // namespace

TEST(TimeFormat, FormatTime) {
  const TimeDelta one_day = TimeDelta::FromDays(1);
  const TimeDelta three_days = TimeDelta::FromDays(3);
  const TimeDelta one_hour = TimeDelta::FromHours(1);
  const TimeDelta four_hours = TimeDelta::FromHours(4);
  const TimeDelta one_min = TimeDelta::FromMinutes(1);
  const TimeDelta three_mins = TimeDelta::FromMinutes(3);
  const TimeDelta one_sec = TimeDelta::FromSeconds(1);
  const TimeDelta five_secs = TimeDelta::FromSeconds(5);
  const TimeDelta twohundred_millisecs = TimeDelta::FromMilliseconds(200);

  // TODO(jungshik) : These test only pass when the OS locale is 'en'.
  // We need to add SetUp() and TearDown() to set the locale to 'en'.
  TestTimeFormats(twohundred_millisecs, "0 secs");
  TestTimeFormats(one_sec - twohundred_millisecs, "0 secs");
  TestTimeFormats(one_sec + twohundred_millisecs, "1 sec");
  TestTimeFormats(five_secs + twohundred_millisecs, "5 secs");
  TestTimeFormats(one_min + five_secs, "1 min");
  TestTimeFormats(three_mins + twohundred_millisecs, "3 mins");
  TestTimeFormats(one_hour + five_secs, "1 hour");
  TestTimeFormats(four_hours + five_secs, "4 hours");
  TestTimeFormats(one_day + five_secs, "1 day");
  TestTimeFormats(three_days, "3 days");
  TestTimeFormats(three_days + four_hours, "3 days");
}
