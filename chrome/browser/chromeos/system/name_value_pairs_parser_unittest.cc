// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system/name_value_pairs_parser.h"

#include "base/basictypes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace system {

TEST(NameValuePairsParser, TestGetSingleValueFromTool) {
  NameValuePairsParser::NameValueMap map;
  NameValuePairsParser parser(&map);
  const char* command[] = { "/bin/echo", "Foo" };
  EXPECT_TRUE(parser.GetSingleValueFromTool(arraysize(command), command,
                                            "foo"));
  ASSERT_EQ(1U, map.size());
  EXPECT_EQ("Foo", map["foo"]);
}

TEST(NameValuePairsParser, TestParseNameValuePairs) {
  NameValuePairsParser::NameValueMap map;
  NameValuePairsParser parser(&map);
  const std::string contents1 = "foo=Foo bar=Bar\nfoobar=FooBar\n";
  EXPECT_TRUE(parser.ParseNameValuePairs(contents1, "=", " \n"));
  ASSERT_EQ(3U, map.size());
  EXPECT_EQ("Foo", map["foo"]);
  EXPECT_EQ("Bar", map["bar"]);
  EXPECT_EQ("FooBar", map["foobar"]);

  map.clear();
  const std::string contents2 = "foo=Foo,bar=Bar";
  EXPECT_TRUE(parser.ParseNameValuePairs(contents2, "=", ",\n"));
  ASSERT_EQ(2U, map.size());
  EXPECT_EQ("Foo", map["foo"]);
  EXPECT_EQ("Bar", map["bar"]);

  map.clear();
  const std::string contents3 = "foo=Foo=foo,bar=Bar";
  EXPECT_FALSE(parser.ParseNameValuePairs(contents3, "=", ",\n"));

  map.clear();
  const std::string contents4 = "foo=Foo,=Bar";
  EXPECT_FALSE(parser.ParseNameValuePairs(contents4, "=", ",\n"));

  map.clear();
  const std::string contents5 =
      "\"initial_locale\"=\"ja\"\n"
      "\"initial_timezone\"=\"Asia/Tokyo\"\n"
      "\"keyboard_layout\"=\"mozc-jp\"\n";
  EXPECT_TRUE(parser.ParseNameValuePairs(contents5, "=", "\n"));
  ASSERT_EQ(3U, map.size());
  EXPECT_EQ("ja", map["initial_locale"]);
  EXPECT_EQ("Asia/Tokyo", map["initial_timezone"]);
  EXPECT_EQ("mozc-jp", map["keyboard_layout"]);
}

TEST(NameValuePairsParser, TestParseNameValuePairsFromTool) {
  // Sample output is taken from the /usr/bin/crosssytem tool.
  const char* command[] = { "/bin/echo",
    "arch                   = x86           # Platform architecture\n" \
    "cros_debug             = 1             # OS should allow debug\n" \
    "dbg_reset              = (error)       # Debug reset mode request\n" \
    "key#with_comment       = some value    # Multiple # comment # delims\n" \
    "key                    =               # No value."
  };

  NameValuePairsParser::NameValueMap map;
  NameValuePairsParser parser(&map);
  parser.ParseNameValuePairsFromTool(
      arraysize(command), command, "=", "\n", "#");
  EXPECT_EQ("x86", map["arch"]);
  EXPECT_EQ("1", map["cros_debug"]);
  EXPECT_EQ("(error)", map["dbg_reset"]);
  EXPECT_EQ("some value", map["key#with_comment"]);
  EXPECT_EQ("", map["key"]);
  EXPECT_EQ(5u, map.size());
}

}  // namespace system
}  // namespace chromeos
