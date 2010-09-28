// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_directory_listing_parser_unittest.h"

#include "base/format_macros.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "net/ftp/ftp_directory_listing_parser_mlsd.h"

namespace {

typedef net::FtpDirectoryListingParserTest FtpDirectoryListingParserMlsdTest;

TEST_F(FtpDirectoryListingParserMlsdTest, Good) {
  base::Time::Exploded now_exploded;
  base::Time::Now().LocalExplode(&now_exploded);

  const struct SingleLineTestData good_cases[] = {
    { "type=file;size=380565;modify=20030606190749; README",
      net::FtpDirectoryListingEntry::FILE, "README", 380565,
      2003, 6, 6, 19, 7 },
    { "type=dir;sizd=512;modify=20031021200128; pub",
      net::FtpDirectoryListingEntry::DIRECTORY, "pub", -1,
      2003, 10, 21, 20, 1 },
    { "type=dir;sizd=512;modify=20091009080706;UNIX.mode=0755; pub",
      net::FtpDirectoryListingEntry::DIRECTORY, "pub", -1,
      2009, 10, 9, 8, 7 },
    { "type=dir;modify=20010414155237;UNIX.mode=0555;unique=6ag5b4e400; etc",
      net::FtpDirectoryListingEntry::DIRECTORY, "etc", -1,
      2001, 4, 14, 15, 52 },
  };
  for (size_t i = 0; i < arraysize(good_cases); i++) {
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]: %s", i,
                                    good_cases[i].input));

    net::FtpDirectoryListingParserMlsd parser;
    RunSingleLineTestCase(&parser, good_cases[i]);
  }
}

TEST_F(FtpDirectoryListingParserMlsdTest, Bad) {
  const char* bad_cases[] = {
    "",
    " ",
    "  ",
    ";",
    "; ",
    " ;",
    " foo",
    "garbage",
    "total 5",
    "type=file;size=380565;modify=20030606190749;README",
    "type=file;size=380565;modify=20030606190749;",
    "type=file;size=380565;modify=20030606190749",
    "size=380565;modify=20030606190749; README",
    "type=file;modify=20030606190749; README",
    "type=file;size=380565; README",
    "type=file; size=380565; modify=20030606190749; README",
    " type=file;size=380565;modify=20030606190749; README",
    "type=file;size=garbage;modify=20030606190749; README",
    "type=file;size=380565;modify=garbage; README",
  };
  for (size_t i = 0; i < arraysize(bad_cases); i++) {
    net::FtpDirectoryListingParserMlsd parser;
    EXPECT_FALSE(parser.ConsumeLine(UTF8ToUTF16(bad_cases[i]))) << bad_cases[i];
  }
}

}  // namespace
