// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/util/character_set_converters.h"

#include <string>

#include "base/basictypes.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

using browser_sync::ToPathString;
using browser_sync::ToUTF8;
using browser_sync::AppendPathStringToUTF8;
using browser_sync::AppendUTF8ToPathString;
using browser_sync::PathStringToUTF8;
using browser_sync::UTF8ToPathString;
using std::string;

class CharacterSetConverterTest : public testing::Test {
};

TEST_F(CharacterSetConverterTest, ASCIIConversionTest) {
  string ascii = "Test String";
  PathString wide = PSTR("Test String");
  ToPathString to_wide(ascii);
  ASSERT_TRUE(to_wide.good());
  ToUTF8 to_utf8(wide);

  // Using == as gunit doesn't handle PathString equality tests correctly (it
  // tries to print the PathString and fails).
  ASSERT_TRUE(PathString(wide) == to_wide.get_string16());
  ASSERT_EQ(string(ascii), to_utf8.get_string());
  ToPathString to_16(ascii);
  ASSERT_TRUE(to_16.good());
  ASSERT_TRUE(PathString(wide) == to_16.get_string16());
#if defined(OS_WIN)
  // On Linux, PathString is already UTF8
  ASSERT_EQ(string(ascii), static_cast<string>(ToUTF8(wide)));
#endif
  // The next line fails the good_checked_ test. It would be a good death test
  // but they don't work on Windows.
  // ASSERT_TRUE(wide == ToPathString(utf8).get_string16());
}

#if defined(OS_WIN)
  // On Linux, PathString is already UTF8
TEST_F(CharacterSetConverterTest, UnicodeConversionText) {
  // Source data obtained by running od -b on files saved in utf-8 and unicode
  // from a text editor.
  const char* utf8 = "\357\273\277\150\145\154\154\157\040\303\250\303\251"
    "\302\251\342\202\254\302\243\302\245\302\256\342\204\242";
// #ifdef IS_LITTLE_ENDIAN
  const PathChar* wide = reinterpret_cast<const PathChar*>("\377\376\150\000"
    "\145\000\154\000\154\000\157\000\040\000\350\000\351\000\251\000\254\040"
    "\243\000\245\000\256\000\042\041");
// #else
//   // This should work, but on Windows we don't have the endian
//   // macros. Since we only do conversion between 16<->8 on Windows,
//   // it's safe to assume little endian.
//   const PathChar* wide =
//     reinterpret_cast<PathChar*>("\376\377\000\150\000\145\000"
//     "\154\000\154\000\157\000\040\000\350\000\351\000\251\040\254\000\243"
//     "\000\245\000\256\041\042");
// #endif

  ToPathString to_wide(utf8);
  ASSERT_TRUE(to_wide.good());
  ToUTF8 to_utf8(wide);

  // Using == as gunit doesn't handle PathString equality tests correctly (it
  // tries to print the PathString and fails).
  ASSERT_TRUE(wide == to_wide.get_string16());
  ASSERT_EQ(string(utf8), to_utf8.get_string());
  ToPathString to_16(utf8);
  ASSERT_TRUE(to_16.good());
  ASSERT_TRUE(wide == to_16.get_string16());
  ASSERT_EQ(string(utf8), reinterpret_cast<const string&>(ToUTF8(wide)));
}
#endif  // defined(OS_WIN)

TEST_F(CharacterSetConverterTest, AppendUTF8Tests) {
  PathString one = PSTR("one");
  PathString two = PSTR("two");
  PathString three = PSTR("three");
  string out;
  AppendPathStringToUTF8(one.data(), one.length(), &out);
  AppendPathStringToUTF8(two.data(), two.length(), &out);
  AppendPathStringToUTF8(three.data(), three.length(), &out);
  ASSERT_EQ(out, "onetwothree");
  PathString onetwothree = PSTR("onetwothree");
  PathStringToUTF8(onetwothree.data(), onetwothree.length(), &out);
  ASSERT_EQ(out, "onetwothree");
}

TEST_F(CharacterSetConverterTest, AppendPathStringTests) {
  string one = "one";
  string two = "two";
  string three = "three";
  PathString out;
  AppendUTF8ToPathString(one.data(), one.length(), &out);
  AppendUTF8ToPathString(two.data(), two.length(), &out);
  AppendUTF8ToPathString(three.data(), three.length(), &out);
  ASSERT_TRUE(out == PathString(PSTR("onetwothree")));
  string onetwothree = "onetwothree";
  UTF8ToPathString(onetwothree.data(), onetwothree.length(), &out);
  ASSERT_TRUE(out == PathString(PSTR("onetwothree")));
}

#if defined(OS_WIN)
namespace {
// See http://en.wikipedia.org/wiki/UTF-16 for an explanation of UTF16.
// For a test case we use the UTF-8 and UTF-16 encoding of char 119070
// (hex 1D11E), which is musical G clef.
const unsigned char utf8_test_string[] = {
  0xEF, 0xBB, 0xBF,  // BOM
  0xE6, 0xB0, 0xB4,  // water, Chinese (0x6C34)
  0x7A,  // lower case z
  0xF0, 0x9D, 0x84, 0x9E,  // musical G clef (0x1D11E)
  0x00,
};
const PathChar utf16_test_string[] = {
  0xFEFF,  // BOM
  0x6C34,  // water, Chinese
  0x007A,  // lower case z
  0xD834, 0xDD1E,  // musical G clef (0x1D11E)
  0x0000,
};
}

TEST_F(CharacterSetConverterTest, UTF16ToUTF8Test) {
  // Avoid truncation warning.
  const char* utf8_test_string_pointer =
    reinterpret_cast<const char*>(utf8_test_string);
  ASSERT_STREQ(utf8_test_string_pointer, ToUTF8(utf16_test_string));
}

TEST_F(CharacterSetConverterTest, utf8_test_stringToUTF16Test) {
  // Avoid truncation warning.
  const char* utf8_test_string_pointer =
    reinterpret_cast<const char*>(utf8_test_string);
  ToPathString converted_utf8(utf8_test_string_pointer);
  ASSERT_TRUE(converted_utf8.good());
  ASSERT_EQ(wcscmp(utf16_test_string, converted_utf8), 0);
}

TEST(NameTruncation, WindowsNameTruncation) {
  using browser_sync::TrimPathStringToValidCharacter;
  PathChar array[] = {'1', '2', 0xD950, 0xDF21, '3', '4', 0};
  PathString message = array;
  ASSERT_EQ(message.length(), arraysize(array) - 1);
  int old_length = message.length();
  while (old_length != 0) {
    TrimPathStringToValidCharacter(&message);
    if (old_length == 4)
      EXPECT_EQ(3, message.length());
    else
      EXPECT_EQ(old_length, message.length());
    message.resize(message.length() - 1);
    old_length = message.length();
  }
  TrimPathStringToValidCharacter(&message);
}
#else

// TODO(zork): Add unittests here once we're running these tests on linux.

#endif
