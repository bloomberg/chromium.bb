// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/port.h"
#include "build/build_config.h"
#include "chrome/browser/sync/syncable/path_name_cmp.h"
#include "chrome/browser/sync/util/path_helpers.h"
#include "chrome/browser/sync/util/sync_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncable {

class PathHelpersTest : public testing::Test {
};

TEST(PathHelpersTest, TruncatePathStringTest) {
  // Simple case.
  PathString str = PSTR("12345");
  EXPECT_EQ(PSTR("123"), TruncatePathString(str, 3));
  EXPECT_EQ(str, TruncatePathString(str, str.size()));

  // abcg is "abc" + musical g clef U+1D11E
#if PATHSTRING_IS_STD_STRING
  // UTF-8
  PathChar abcg[] = {'a', 'b', 'c', 0xF0, 0x9D, 0x84, 0x9E, '\0'};
#else  // PATHSTRING_IS_STD_STRING
  // UTF-16
  PathChar abcg[] = {'a', 'b', 'c', 0xD834, 0xDD1E, '\0'};
#endif  // PATHSTRING_IS_STD_STRING

  EXPECT_EQ(PSTR("abc"), TruncatePathString(abcg, 4));

  // Further utf-8 tests.
#if PATHSTRING_IS_STD_STRING
  // UTF-8

  EXPECT_EQ(PSTR("abc"), TruncatePathString(abcg, 4));
  EXPECT_EQ(PSTR("abc"), TruncatePathString(abcg, 5));
  EXPECT_EQ(PSTR("abc"), TruncatePathString(abcg, 6));
  EXPECT_EQ(PathString(abcg), TruncatePathString(abcg, 7));

  PathChar abc2[] = {'a', 'b', 'c', 0xC3, 0xB1, '\0'};  // abc(n w/ tilde)
  EXPECT_EQ(PSTR("abc"), TruncatePathString(abc2, 3));
  EXPECT_EQ(PSTR("abc"), TruncatePathString(abc2, 4));
  EXPECT_EQ(PathString(abc2), TruncatePathString(abc2, 5));

  PathChar abc3[] = {'a', 'b', 'c', 0xE2, 0x82, 0xAC, '\0'};  // abc(euro)
  EXPECT_EQ(PSTR("abc"), TruncatePathString(abc3, 3));
  EXPECT_EQ(PSTR("abc"), TruncatePathString(abc3, 4));
  EXPECT_EQ(PSTR("abc"), TruncatePathString(abc3, 5));
  EXPECT_EQ(PathString(abc3), TruncatePathString(abc3, 6));
#endif
}

TEST(PathHelpersTest, PathStrutil) {
  PathString big = PSTR("abcdef");
  PathString suffix = PSTR("def");
  PathString other = PSTR("x");
  EXPECT_TRUE(HasSuffixPathString(big, suffix));
  EXPECT_FALSE(HasSuffixPathString(suffix, big));
  EXPECT_FALSE(HasSuffixPathString(big, other));
  EXPECT_EQ(PSTR("abc"), StripSuffixPathString(big, suffix));
}

TEST(PathHelpersTest, SanitizePathComponent) {
#if defined(OS_WIN)
  EXPECT_EQ(MakePathComponentOSLegal(L"bar"), L"");
  EXPECT_EQ(MakePathComponentOSLegal(L"bar <"), L"bar");
  EXPECT_EQ(MakePathComponentOSLegal(L"bar.<"), L"bar");
  EXPECT_EQ(MakePathComponentOSLegal(L"prn"), L"prn~1");
  EXPECT_EQ(MakePathComponentOSLegal(L"pr>n"), L"prn~1");
  EXPECT_EQ(MakePathComponentOSLegal(L"ab:c"), L"abc");
  EXPECT_EQ(MakePathComponentOSLegal(L"a|bc"), L"abc");
  EXPECT_EQ(MakePathComponentOSLegal(L"baz~9"), L"");
  EXPECT_EQ(MakePathComponentOSLegal(L"\007"), L"~1");
  EXPECT_EQ(MakePathComponentOSLegal(L"com1.txt.bat"), L"com1~1.txt.bat");
  EXPECT_EQ(MakePathComponentOSLegal(L"foo.com1.bat"), L"");
  EXPECT_EQ(MakePathComponentOSLegal(L"\010gg"), L"gg");
  EXPECT_EQ(MakePathComponentOSLegal(L"<"), L"~1");
  EXPECT_EQ(MakePathComponentOSLegal(L"col:on"), L"colon");
  EXPECT_EQ(MakePathComponentOSLegal(L"q\""), L"q");
  EXPECT_EQ(MakePathComponentOSLegal(L"back\\slAsh"), L"backslAsh");
  EXPECT_EQ(MakePathComponentOSLegal(L"sla/sh "), L"slash");
  EXPECT_EQ(MakePathComponentOSLegal(L"s|laSh"), L"slaSh");
  EXPECT_EQ(MakePathComponentOSLegal(L"CON"), L"CON~1");
  EXPECT_EQ(MakePathComponentOSLegal(L"PRN"), L"PRN~1");
  EXPECT_EQ(MakePathComponentOSLegal(L"AUX"), L"AUX~1");
  EXPECT_EQ(MakePathComponentOSLegal(L"NUL"), L"NUL~1");
  EXPECT_EQ(MakePathComponentOSLegal(L"COM1"), L"COM1~1");
  EXPECT_EQ(MakePathComponentOSLegal(L"COM2"), L"COM2~1");
  EXPECT_EQ(MakePathComponentOSLegal(L"COM3"), L"COM3~1");
  EXPECT_EQ(MakePathComponentOSLegal(L"COM4"), L"COM4~1");
  EXPECT_EQ(MakePathComponentOSLegal(L"COM5"), L"COM5~1");
  EXPECT_EQ(MakePathComponentOSLegal(L"COM6"), L"COM6~1");
  EXPECT_EQ(MakePathComponentOSLegal(L"COM7"), L"COM7~1");
  EXPECT_EQ(MakePathComponentOSLegal(L"COM8"), L"COM8~1");
  EXPECT_EQ(MakePathComponentOSLegal(L"COM9"), L"COM9~1");
  EXPECT_EQ(MakePathComponentOSLegal(L"LPT1"), L"LPT1~1");
  EXPECT_EQ(MakePathComponentOSLegal(L"LPT2"), L"LPT2~1");
  EXPECT_EQ(MakePathComponentOSLegal(L"LPT3"), L"LPT3~1");
  EXPECT_EQ(MakePathComponentOSLegal(L"LPT4"), L"LPT4~1");
  EXPECT_EQ(MakePathComponentOSLegal(L"LPT5"), L"LPT5~1");
  EXPECT_EQ(MakePathComponentOSLegal(L"LPT6"), L"LPT6~1");
  EXPECT_EQ(MakePathComponentOSLegal(L"LPT7"), L"LPT7~1");
  EXPECT_EQ(MakePathComponentOSLegal(L"LPT8"), L"LPT8~1");
  EXPECT_EQ(MakePathComponentOSLegal(L"LPT9"), L"LPT9~1");
  EXPECT_EQ(MakePathComponentOSLegal(L"bar~bar"), L"");
  EXPECT_EQ(MakePathComponentOSLegal(L"adlr~-3"), L"");
  EXPECT_EQ(MakePathComponentOSLegal(L"tilde~"), L"");
  EXPECT_EQ(MakePathComponentOSLegal(L"mytext.txt"), L"");
  EXPECT_EQ(MakePathComponentOSLegal(L"mytext|.txt"), L"mytext.txt");
  EXPECT_EQ(MakePathComponentOSLegal(L"okay.com1.txt"), L"");
  EXPECT_EQ(MakePathComponentOSLegal(L"software-3.tar.gz"), L"");
  EXPECT_EQ(MakePathComponentOSLegal(L"<"), L"~1");
  EXPECT_EQ(MakePathComponentOSLegal(L"<.<"), L"~1");
  EXPECT_EQ(MakePathComponentOSLegal(L"<.<txt"), L".txt");
  EXPECT_EQ(MakePathComponentOSLegal(L"txt<.<"), L"txt");
#else  // !defined(OS_WIN)

  EXPECT_EQ(MakePathComponentOSLegal("bar"), "");
  EXPECT_EQ(MakePathComponentOSLegal("b"), "");
  EXPECT_EQ(MakePathComponentOSLegal("A"), "");
  EXPECT_EQ(MakePathComponentOSLegal("<'|"), "");
  EXPECT_EQ(MakePathComponentOSLegal("/"), ":");
  EXPECT_EQ(MakePathComponentOSLegal(":"), "");

#endif  // defined(OS_WIN)
}

}  // namespace syncable
