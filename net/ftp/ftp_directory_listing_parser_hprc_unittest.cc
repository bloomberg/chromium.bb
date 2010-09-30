// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_directory_listing_parser_unittest.h"

#include "base/format_macros.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "net/ftp/ftp_directory_listing_parser_hprc.h"

namespace {

typedef net::FtpDirectoryListingParserTest FtpDirectoryListingParserHprcTest;

TEST_F(FtpDirectoryListingParserHprcTest, Good) {
  const struct SingleLineTestData good_cases[] = {
    { " .welcome",
      net::FtpDirectoryListingEntry::FILE, ".welcome", 0,
      1994, 11, 15, 12, 45 },
  };
  for (size_t i = 0; i < arraysize(good_cases); i++) {
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]: %s", i,
                                    good_cases[i].input));

    net::FtpDirectoryListingParserHprc parser(GetMockCurrentTime());
    RunSingleLineTestCase(&parser, good_cases[i]);
  }
}

TEST_F(FtpDirectoryListingParserHprcTest, Bad) {
  const char* bad_cases[] = {
    "",
    "test",
    "-rw-r--r--    1 ftp      ftp           528 Nov 01  2007 README",
    "d [RWCEAFMS] ftpadmin 512 Jan 29  2004 pub",
    "TEST.DIR;1 1 4-MAR-1999 22:14:34 [UCX$NOBO,ANONYMOUS] (RWE,RWE,RWE,RWE)",
    "type=dir;modify=20010414155237;UNIX.mode=0555;unique=6ag5b4e400; etc",
  };
  for (size_t i = 0; i < arraysize(bad_cases); i++) {
    net::FtpDirectoryListingParserHprc parser(GetMockCurrentTime());
    EXPECT_FALSE(parser.ConsumeLine(UTF8ToUTF16(bad_cases[i]))) << bad_cases[i];
  }
}

}  // namespace
