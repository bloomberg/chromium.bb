// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_util.h"

#include "base/file_path.h"
#include "base/i18n/time_formatting.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/system/timezone_settings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gdata {
namespace util {

namespace {

std::string FormatTime(const base::Time& time) {
  return UTF16ToUTF8(TimeFormatShortDateAndTime(time));
}

}  // namespace

TEST(GDataUtilTest, IsUnderGDataMountPoint) {
  EXPECT_FALSE(IsUnderGDataMountPoint(
      FilePath::FromUTF8Unsafe("/wherever/foo.txt")));
  EXPECT_FALSE(IsUnderGDataMountPoint(
      FilePath::FromUTF8Unsafe("/special/foo.txt")));
  EXPECT_FALSE(IsUnderGDataMountPoint(
      FilePath::FromUTF8Unsafe("/special/drivex/foo.txt")));
  EXPECT_FALSE(IsUnderGDataMountPoint(
      FilePath::FromUTF8Unsafe("special/drivex/foo.txt")));

  EXPECT_TRUE(IsUnderGDataMountPoint(
      FilePath::FromUTF8Unsafe("/special/drive")));
  EXPECT_TRUE(IsUnderGDataMountPoint(
      FilePath::FromUTF8Unsafe("/special/drive/foo.txt")));
  EXPECT_TRUE(IsUnderGDataMountPoint(
      FilePath::FromUTF8Unsafe("/special/drive/subdir/foo.txt")));
}

TEST(GDataUtilTest, ExtractGDataPath) {
  EXPECT_EQ(FilePath(),
            ExtractGDataPath(
                FilePath::FromUTF8Unsafe("/wherever/foo.txt")));
  EXPECT_EQ(FilePath(),
            ExtractGDataPath(
                FilePath::FromUTF8Unsafe("/special/foo.txt")));
  EXPECT_EQ(FilePath(),
            ExtractGDataPath(
                FilePath::FromUTF8Unsafe("/special/gdatax/foo.txt")));

  EXPECT_EQ(FilePath::FromUTF8Unsafe("drive"),
            ExtractGDataPath(
                FilePath::FromUTF8Unsafe("/special/drive")));
  EXPECT_EQ(FilePath::FromUTF8Unsafe("drive/foo.txt"),
            ExtractGDataPath(
                FilePath::FromUTF8Unsafe("/special/drive/foo.txt")));
  EXPECT_EQ(FilePath::FromUTF8Unsafe("drive/subdir/foo.txt"),
            ExtractGDataPath(
                FilePath::FromUTF8Unsafe("/special/drive/subdir/foo.txt")));
}

TEST(GDataUtilTest, EscapeUnescapeCacheFileName) {
  const std::string kUnescapedFileName(
      "tmp:`~!@#$%^&*()-_=+[{|]}\\\\;\',<.>/?");
  const std::string kEscapedFileName(
      "tmp:`~!@#$%25^&*()-_=+[{|]}\\\\;\',<%2E>%2F?");
  EXPECT_EQ(kEscapedFileName, EscapeCacheFileName(kUnescapedFileName));
  EXPECT_EQ(kUnescapedFileName, UnescapeCacheFileName(kEscapedFileName));
}

TEST(GDataUtilTest, ParseCacheFilePath) {
  std::string resource_id, md5, extra_extension;
  ParseCacheFilePath(
      FilePath::FromUTF8Unsafe(
          "/home/user/GCache/v1/persistent/pdf:a1b2.0123456789abcdef.mounted"),
      &resource_id,
      &md5,
      &extra_extension);
  EXPECT_EQ(resource_id, "pdf:a1b2");
  EXPECT_EQ(md5, "0123456789abcdef");
  EXPECT_EQ(extra_extension, "mounted");

  ParseCacheFilePath(
      FilePath::FromUTF8Unsafe(
          "/home/user/GCache/v1/tmp/pdf:a1b2.0123456789abcdef"),
      &resource_id,
      &md5,
      &extra_extension);
  EXPECT_EQ(resource_id, "pdf:a1b2");
  EXPECT_EQ(md5, "0123456789abcdef");
  EXPECT_EQ(extra_extension, "");

  ParseCacheFilePath(
      FilePath::FromUTF8Unsafe(
          "/home/user/GCache/v1/pinned/pdf:a1b2"),
      &resource_id,
      &md5,
      &extra_extension);
  EXPECT_EQ(resource_id, "pdf:a1b2");
  EXPECT_EQ(md5, "");
  EXPECT_EQ(extra_extension, "");
}

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

}  // namespace util
}  // namespace gdata
