// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_directory_listing_parsers.h"

#include "base/format_macros.h"
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
    SCOPED_TRACE(StringPrintf("Test[%" PRIuS "]: %s", i, good_cases[i].input));

    net::FtpLsDirectoryListingParser parser;
    RunSingleLineTestCase(&parser, good_cases[i]);
  }

  const char* bad_cases[] = {
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

TEST_F(FtpDirectoryListingParsersTest, Windows) {
  base::Time::Exploded now_exploded;
  base::Time::Now().LocalExplode(&now_exploded);

  const struct SingleLineTestData good_cases[] = {
    { "11-02-09  05:32PM       <DIR>          NT",
      net::FtpDirectoryListingEntry::DIRECTORY, "NT", -1,
      2009, 11, 2, 17, 32 },
    { "01-06-09  02:42PM                  458 Readme.txt",
      net::FtpDirectoryListingEntry::FILE, "Readme.txt", 458,
      2009, 1, 6, 14, 42 },
    { "01-06-09  02:42AM                  1 Readme.txt",
      net::FtpDirectoryListingEntry::FILE, "Readme.txt", 1,
      2009, 1, 6, 2, 42 },
    { "01-06-01  02:42AM                  458 Readme.txt",
      net::FtpDirectoryListingEntry::FILE, "Readme.txt", 458,
      2001, 1, 6, 2, 42 },
    { "01-06-00  02:42AM                  458 Corner1.txt",
      net::FtpDirectoryListingEntry::FILE, "Corner1.txt", 458,
      2000, 1, 6, 2, 42 },
    { "01-06-99  02:42AM                  458 Corner2.txt",
      net::FtpDirectoryListingEntry::FILE, "Corner2.txt", 458,
      1999, 1, 6, 2, 42 },
    { "01-06-80  02:42AM                  458 Corner3.txt",
      net::FtpDirectoryListingEntry::FILE, "Corner3.txt", 458,
      1980, 1, 6, 2, 42 },
#if !defined(OS_LINUX)
    // TODO(phajdan.jr): Re-enable when 2038-year problem is fixed on Linux.
    { "01-06-79  02:42AM                  458 Corner4",
      net::FtpDirectoryListingEntry::FILE, "Corner4", 458,
      2079, 1, 6, 2, 42 },
#endif  // !defined (OS_LINUX)
    { "01-06-1979  02:42AM                458 Readme.txt",
      net::FtpDirectoryListingEntry::FILE, "Readme.txt", 458,
      1979, 1, 6, 2, 42 },
  };
  for (size_t i = 0; i < arraysize(good_cases); i++) {
    SCOPED_TRACE(StringPrintf("Test[%" PRIuS "]: %s", i, good_cases[i].input));

    net::FtpWindowsDirectoryListingParser parser;
    RunSingleLineTestCase(&parser, good_cases[i]);
  }

  const char* bad_cases[] = {
    "",
    "garbage",
    "11-02-09  05:32PM       <GARBAGE>      NT",
    "11-02-09  05:32         <DIR>          NT",
    "11-FEB-09 05:32PM       <DIR>          NT",
    "11-02     05:32PM       <DIR>          NT",
    "11-02-09  05:32PM                 -1   NT",
  };
  for (size_t i = 0; i < arraysize(bad_cases); i++) {
    net::FtpWindowsDirectoryListingParser parser;
    EXPECT_FALSE(parser.ConsumeLine(UTF8ToUTF16(bad_cases[i]))) << bad_cases[i];
  }
}

TEST_F(FtpDirectoryListingParsersTest, Vms) {
  const struct SingleLineTestData good_cases[] = {
    { "README.TXT;4  2  18-APR-2000 10:40:39.90",
      net::FtpDirectoryListingEntry::FILE, "readme.txt", 1024,
      2000, 4, 18, 10, 40 },
    { ".WELCOME;1    2  13-FEB-2002 23:32:40.47",
      net::FtpDirectoryListingEntry::FILE, ".welcome", 1024,
      2002, 2, 13, 23, 32 },
    { "FILE.;1    2  13-FEB-2002 23:32:40.47",
      net::FtpDirectoryListingEntry::FILE, "file.", 1024,
      2002, 2, 13, 23, 32 },
    { "EXAMPLE.TXT;1  1   4-NOV-2009 06:02 [JOHNDOE] (RWED,RWED,,)",
      net::FtpDirectoryListingEntry::FILE, "example.txt", 512,
      2009, 11, 4, 6, 2 },
    { "ANNOUNCE.TXT;2 1/16 12-MAR-2005 08:44:57 [SYSTEM] (RWED,RWED,RE,RE)",
      net::FtpDirectoryListingEntry::FILE, "announce.txt", 512,
      2005, 3, 12, 8, 44 },
    { "TEST.DIR;1 1 4-MAR-1999 22:14:34 [UCX$NOBO,ANONYMOUS] (RWE,RWE,RWE,RWE)",
      net::FtpDirectoryListingEntry::DIRECTORY, "test", -1,
      1999, 3, 4, 22, 14 },
    { "ANNOUNCE.TXT;2 1 12-MAR-2005 08:44:57 [X] (,,,)",
      net::FtpDirectoryListingEntry::FILE, "announce.txt", 512,
      2005, 3, 12, 8, 44 },
    { "ANNOUNCE.TXT;2 1 12-MAR-2005 08:44:57 [X] (R,RW,RWD,RE)",
      net::FtpDirectoryListingEntry::FILE, "announce.txt", 512,
      2005, 3, 12, 8, 44 },
    { "ANNOUNCE.TXT;2 1 12-MAR-2005 08:44:57 [X] (ED,RED,WD,WED)",
      net::FtpDirectoryListingEntry::FILE, "announce.txt", 512,
      2005, 3, 12, 8, 44 },
  };
  for (size_t i = 0; i < arraysize(good_cases); i++) {
    SCOPED_TRACE(StringPrintf("Test[%" PRIuS "]: %s", i, good_cases[i].input));

    net::FtpVmsDirectoryListingParser parser;
    ASSERT_TRUE(
        parser.ConsumeLine(ASCIIToUTF16("Directory ANONYMOUS_ROOT:[000000]")));
    RunSingleLineTestCase(&parser, good_cases[i]);
  }

  const char* bad_cases[] = {
    "Directory ROOT|garbage",

    // Missing file version number.
    "Directory ROOT|README.TXT 2 18-APR-2000 10:40:39",

    // Missing extension.
    "Directory ROOT|README;1 2 18-APR-2000 10:40:39",

    // Malformed file size.
    "Directory ROOT|README.TXT;1 garbage 18-APR-2000 10:40:39",
    "Directory ROOT|README.TXT;1 -2 18-APR-2000 10:40:39",

    // Malformed date.
    "Directory ROOT|README.TXT;1 2 APR-2000 10:40:39",
    "Directory ROOT|README.TXT;1 2 -18-APR-2000 10:40:39",
    "Directory ROOT|README.TXT;1 2 18-APR 10:40:39",
    "Directory ROOT|README.TXT;1 2 18-APR-2000 10",
    "Directory ROOT|README.TXT;1 2 18-APR-2000 10:40.25",
    "Directory ROOT|README.TXT;1 2 18-APR-2000 10:40.25.25",

    // Empty line inside the listing.
    "Directory ROOT|README.TXT;1 2 18-APR-2000 10:40:42"
    "||README.TXT;1 2 18-APR-2000 10:40:42",

    // Data after footer.
    "Directory ROOT|README.TXT;4 2 18-APR-2000 10:40:39"
    "||Total of 1 file|",
    "Directory ROOT|README.TXT;4 2 18-APR-2000 10:40:39"
    "||Total of 1 file|garbage",
    "Directory ROOT|README.TXT;4 2 18-APR-2000 10:40:39"
    "||Total of 1 file|Total of 1 file",

    // Malformed security information.
    "Directory ROOT|X.TXT;2 1 12-MAR-2005 08:44:57 (RWED,RWED,RE,RE)",
    "Directory ROOT|X.TXT;2 1 12-MAR-2005 08:44:57 [SYSTEM]",
    "Directory ROOT|X.TXT;2 1 12-MAR-2005 08:44:57 (SYSTEM) (RWED,RWED,RE,RE)",
    "Directory ROOT|X.TXT;2 1 12-MAR-2005 08:44:57 [SYSTEM] [RWED,RWED,RE,RE]",
    "Directory ROOT|X.TXT;2 1 12-MAR-2005 08:44:57 [X] (RWED)",
    "Directory ROOT|X.TXT;2 1 12-MAR-2005 08:44:57 [X] (RWED,RWED,RE,RE,RE)",
    "Directory ROOT|X.TXT;2 1 12-MAR-2005 08:44:57 [X] (RWED,RWEDRWED,RE,RE)",
    "Directory ROOT|X.TXT;2 1 12-MAR-2005 08:44:57 [X] (RWED,DEWR,RE,RE)",
    "Directory ROOT|X.TXT;2 1 12-MAR-2005 08:44:57 [X] (RWED,RWED,Q,RE)",
    "Directory ROOT|X.TXT;2 1 12-MAR-2005 08:44:57 [X] (RWED,RRWWEEDD,RE,RE)",
  };
  for (size_t i = 0; i < arraysize(bad_cases); i++) {
    SCOPED_TRACE(StringPrintf("Test[%" PRIuS "]: %s", i, bad_cases[i]));

    std::vector<std::string> lines;
    SplitString(bad_cases[i], '|', &lines);
    net::FtpVmsDirectoryListingParser parser;
    bool failed = false;
    for (std::vector<std::string>::const_iterator i = lines.begin();
         i != lines.end(); ++i) {
      if (!parser.ConsumeLine(UTF8ToUTF16(*i))) {
        failed = true;
        break;
      }
    }
    EXPECT_TRUE(failed);
  }
}

}  // namespace
