// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_util.h"

#include "base/i18n/time_formatting.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/system/timezone_settings.h"
#endif  // OS_CHROMEOS

namespace gdata {
namespace util {
namespace {

std::string FormatTime(const base::Time& time) {
  return UTF16ToUTF8(TimeFormatShortDateAndTime(time));
}

}  // namespace

// TODO(yoshiki): Find platform independent way to get/set local timezone.
// (http://crbug.com/147524).
#if defined(OS_CHROMEOS)
TEST(GDataUtilTest, GetTimeFromStringLocalTimezone) {
  // Creates time object GMT.
  base::Time::Exploded exploded = {2012, 7, 0, 14, 1, 3, 21, 151};
  base::Time target_time = base::Time::FromUTCExploded(exploded);

  // Creates time object as the local time.
  base::Time test_time;
  ASSERT_TRUE(GetTimeFromString("2012-07-14T01:03:21.151", &test_time));

  // Gets the offset between the local time and GMT.
  const icu::TimeZone& tz =
      chromeos::system::TimezoneSettings::GetInstance()->GetTimezone();
  UErrorCode status = U_ZERO_ERROR;
  int millisecond_in_day = ((1 * 60 + 3) * 60 + 21) * 1000 + 151;
  int offset = tz.getOffset(1, 2012, 7, 14, 1, millisecond_in_day, status);
  ASSERT_TRUE(U_SUCCESS(status));

  EXPECT_EQ((target_time - test_time).InMilliseconds(), offset);
}

TEST(GDataUtilTest, GetTimeFromStringTimezone) {
  // Sets the current timezone to GMT.
  chromeos::system::TimezoneSettings::GetInstance()->
      SetTimezone(*icu::TimeZone::getGMT());

  base::Time target_time;
  base::Time test_time;
  // Creates the target time.
  EXPECT_TRUE(GetTimeFromString("2012-07-14T01:03:21.151Z", &target_time));

  // Tests positive offset (hour only).
  EXPECT_TRUE(GetTimeFromString("2012-07-14T02:03:21.151+01", &test_time));
  EXPECT_EQ(FormatTime(target_time), FormatTime(test_time));

  // Tests positive offset (hour and minutes).
  EXPECT_TRUE(GetTimeFromString("2012-07-14T07:33:21.151+06:30", &test_time));
  EXPECT_EQ(FormatTime(target_time), FormatTime(test_time));

  // Tests negative offset.
  EXPECT_TRUE(GetTimeFromString("2012-07-13T18:33:21.151-06:30", &test_time));
  EXPECT_EQ(FormatTime(target_time), FormatTime(test_time));
}

TEST(GDataUtilTest, GetTimeFromString) {
  // Sets the current timezone to GMT.
  chromeos::system::TimezoneSettings::GetInstance()->
      SetTimezone(*icu::TimeZone::getGMT());

  base::Time test_time;

  base::Time::Exploded target_time1 = {2005, 1, 0, 7, 8, 2, 0, 0};
  EXPECT_TRUE(GetTimeFromString("2005-01-07T08:02:00Z", &test_time));
  EXPECT_EQ(FormatTime(base::Time::FromUTCExploded(target_time1)),
            FormatTime(test_time));

  base::Time::Exploded target_time2 = {2005, 8, 0, 9, 17, 57, 0, 0};
  EXPECT_TRUE(GetTimeFromString("2005-08-09T09:57:00-08:00", &test_time));
  EXPECT_EQ(FormatTime(base::Time::FromUTCExploded(target_time2)),
            FormatTime(test_time));

  base::Time::Exploded target_time3 = {2005, 1, 0, 7, 8, 2, 0, 123};
  EXPECT_TRUE(GetTimeFromString("2005-01-07T08:02:00.123Z", &test_time));
  EXPECT_EQ(FormatTime(base::Time::FromUTCExploded(target_time3)),
            FormatTime(test_time));
}
#endif  // OS_CHROMEOS

TEST(GDataUtilTest, FormatTimeAsString) {
  base::Time::Exploded exploded_time = {2012, 7, 0, 19, 15, 59, 13, 123};
  base::Time time = base::Time::FromUTCExploded(exploded_time);
  EXPECT_EQ("2012-07-19T15:59:13.123Z", FormatTimeAsString(time));
}

}  // namespace util
}  // namespace gdata
