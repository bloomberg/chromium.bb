// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_directory_listing_parsers.h"

#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
  
struct SingleLineTestData {
  const char* input;
  net::FtpDirectoryListingEntry::Type type;
  const char* filename;
  int64 size;
  int year;
  int month;
  int day_of_month;
  int hour;
  int minute;
};

class FtpDirectoryListingParsersTest : public testing::Test {
 protected:
  FtpDirectoryListingParsersTest() {
  }
  
  void RunSingleLineTestCase(net::FtpDirectoryListingParser* parser,
                             const SingleLineTestData& test_case) {
    ASSERT_TRUE(parser->ConsumeLine(UTF8ToUTF16(test_case.input)));
    ASSERT_TRUE(parser->EntryAvailable());
    net::FtpDirectoryListingEntry entry = parser->PopEntry();
    EXPECT_EQ(test_case.type, entry.type);
    EXPECT_EQ(UTF8ToUTF16(test_case.filename), entry.name);
    EXPECT_EQ(test_case.size, entry.size);
    
    base::Time::Exploded time_exploded;
    entry.last_modified.LocalExplode(&time_exploded);
    EXPECT_EQ(test_case.year, time_exploded.year);
    EXPECT_EQ(test_case.month, time_exploded.month);
    EXPECT_EQ(test_case.day_of_month, time_exploded.day_of_month);
    EXPECT_EQ(test_case.hour, time_exploded.hour);
    EXPECT_EQ(test_case.minute, time_exploded.minute);
    EXPECT_EQ(0, time_exploded.second);
    EXPECT_EQ(0, time_exploded.millisecond);
  }
  
 private:
  DISALLOW_COPY_AND_ASSIGN(FtpDirectoryListingParsersTest);
};

TEST_F(FtpDirectoryListingParsersTest, Ls) {
  base::Time::Exploded now_exploded;
  base::Time::Now().LocalExplode(&now_exploded);
  
  const struct SingleLineTestData good_cases[] = {
    { "-rw-r--r--    1 ftp      ftp           528 Nov 01  2007 README",
      net::FtpDirectoryListingEntry::FILE, "README", 528,
      2007, 11, 1, 0, 0 },
    { "drwxr-xr-x    3 ftp      ftp          4096 May 15 18:11 directory",
      net::FtpDirectoryListingEntry::DIRECTORY, "directory", -1,
      now_exploded.year, 5, 15, 18, 11 },
    { "lrwxrwxrwx 1 0  0 26 Sep 18 2008 pub -> vol/1/.CLUSTER/var_ftp/pub",
      net::FtpDirectoryListingEntry::SYMLINK, "pub", -1,
      2008, 9, 18, 0, 0 },
    { "lrwxrwxrwx 1 0  0 3 Oct 12 13:37 mirror -> pub",
      net::FtpDirectoryListingEntry::SYMLINK, "mirror", -1,
      now_exploded.year, 10, 12, 13, 37 },
  };
  for (size_t i = 0; i < arraysize(good_cases); i++) {
    SCOPED_TRACE(StringPrintf("Test[%d]: %s", i, good_cases[i].input));
    
    net::FtpLsDirectoryListingParser parser;
    RunSingleLineTestCase(&parser, good_cases[i]);
  }
  
  const char* bad_cases[] = {
    "",
    "garbage",
    "-rw-r--r-- 1 ftp ftp",
    "-rw-r--rgb 1 ftp ftp 528 Nov 01 2007 README",
    "-rw-rgbr-- 1 ftp ftp 528 Nov 01 2007 README",
    "qrwwr--r-- 1 ftp ftp 528 Nov 01 2007 README",
    "-rw-r--r-- -1 ftp ftp 528 Nov 01 2007 README",
    "-rw-r--r-- 1 ftp ftp -528 Nov 01 2007 README",
    "-rw-r--r-- 1 ftp ftp 528 Foo 01 2007 README",
  };
  for (size_t i = 0; i < arraysize(bad_cases); i++) {
    net::FtpLsDirectoryListingParser parser;
    EXPECT_FALSE(parser.ConsumeLine(UTF8ToUTF16(bad_cases[i]))) << bad_cases[i];
  }
}

}  // namespace
