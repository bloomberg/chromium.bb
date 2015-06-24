// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <time.h>

#include "chromecast/crash/linux/dump_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {

TEST(DumpInfoTest, EmptyStringIsNotValid) {
  DumpInfo dump_info("");
  ASSERT_FALSE(dump_info.valid());
}

TEST(DumpInfoTest, TooFewFieldsIsNotValid) {
  DumpInfo dump_info("name|2001-11-12 18:31:01|dump_string");
  ASSERT_FALSE(dump_info.valid());
}

TEST(DumpInfoTest, BadTimeStringIsNotValid) {
  DumpInfo info("name|Mar 23 2014 01:23:45|dump_string|123456789|logfile.log");
  ASSERT_FALSE(info.valid());
}

TEST(DumpInfoTest, AllRequiredFieldsIsValid) {
  DumpInfo info("name|2001-11-12 18:31:01|dump_string|123456789|logfile.log");
  struct tm tm = {0};
  tm.tm_isdst = 0;
  tm.tm_sec = 1;
  tm.tm_min = 31;
  tm.tm_hour = 18;
  tm.tm_mday = 12;
  tm.tm_mon = 10;
  tm.tm_year = 101;
  time_t dump_time = mktime(&tm);

  ASSERT_TRUE(info.valid());
  ASSERT_EQ("name", info.params().process_name);
  ASSERT_EQ(dump_time, info.dump_time());
  ASSERT_EQ("dump_string", info.crashed_process_dump());
  ASSERT_EQ(123456789u, info.params().process_uptime);
  ASSERT_EQ("logfile.log", info.logfile());
}

TEST(DumpInfoTest, EmptyProcessNameIsValid) {
  DumpInfo dump_info("|2001-11-12 18:31:01|dump_string|123456789|logfile.log");
  ASSERT_TRUE(dump_info.valid());
}

TEST(DumpInfoTest, SomeRequiredFieldsEmptyIsValid) {
  DumpInfo info("name|2001-11-12 18:31:01|||");
  struct tm tm = {0};
  tm.tm_isdst = 0;
  tm.tm_sec = 1;
  tm.tm_min = 31;
  tm.tm_hour = 18;
  tm.tm_mday = 12;
  tm.tm_mon = 10;
  tm.tm_year = 101;
  time_t dump_time = mktime(&tm);

  ASSERT_TRUE(info.valid());
  ASSERT_EQ("name", info.params().process_name);
  ASSERT_EQ(dump_time, info.dump_time());
  ASSERT_EQ("", info.crashed_process_dump());
  ASSERT_EQ(0u, info.params().process_uptime);
  ASSERT_EQ("", info.logfile());
}

TEST(DumpInfoTest, AllOptionalFieldsIsValid) {
  DumpInfo info(
      "name|2001-11-12 18:31:01|dump_string|123456789|logfile.log|"
      "suffix|previous_app|current_app|last_app|RELEASE|BUILD_NUMBER");
  struct tm tm = {0};
  tm.tm_isdst = 0;
  tm.tm_sec = 1;
  tm.tm_min = 31;
  tm.tm_hour = 18;
  tm.tm_mday = 12;
  tm.tm_mon = 10;
  tm.tm_year = 101;
  time_t dump_time = mktime(&tm);

  ASSERT_TRUE(info.valid());
  ASSERT_EQ("name", info.params().process_name);
  ASSERT_EQ(dump_time, info.dump_time());
  ASSERT_EQ("dump_string", info.crashed_process_dump());
  ASSERT_EQ(123456789u, info.params().process_uptime);
  ASSERT_EQ("logfile.log", info.logfile());

  ASSERT_EQ("suffix", info.params().suffix);
  ASSERT_EQ("previous_app", info.params().previous_app_name);
  ASSERT_EQ("current_app", info.params().current_app_name);
  ASSERT_EQ("last_app", info.params().last_app_name);
}

TEST(DumpInfoTest, SomeOptionalFieldsIsValid) {
  DumpInfo info(
      "name|2001-11-12 18:31:01|dump_string|123456789|logfile.log|"
      "suffix|previous_app");
  struct tm tm = {0};
  tm.tm_isdst = 0;
  tm.tm_sec = 1;
  tm.tm_min = 31;
  tm.tm_hour = 18;
  tm.tm_mday = 12;
  tm.tm_mon = 10;
  tm.tm_year = 101;
  time_t dump_time = mktime(&tm);

  ASSERT_TRUE(info.valid());
  ASSERT_EQ("name", info.params().process_name);
  ASSERT_EQ(dump_time, info.dump_time());
  ASSERT_EQ("dump_string", info.crashed_process_dump());
  ASSERT_EQ(123456789u, info.params().process_uptime);
  ASSERT_EQ("logfile.log", info.logfile());

  ASSERT_EQ("suffix", info.params().suffix);
  ASSERT_EQ("previous_app", info.params().previous_app_name);
}

TEST(DumpInfoTest, TooManyFieldsIsNotValid) {
  DumpInfo info(
      "name|2001-11-12 18:31:01|dump_string|123456789|logfile.log|"
      "suffix|previous_app|current_app|last_app|VERSION|BUILD_NUM|extra_field");
  ASSERT_FALSE(info.valid());
}

}  // chromecast