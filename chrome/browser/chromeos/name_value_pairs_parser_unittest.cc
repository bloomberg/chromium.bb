// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/name_value_pairs_parser.h"

#include "base/basictypes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

TEST(NameValuePairsParser, TestGetSingleValueFromTool) {
  NameValuePairsParser::NameValueMap map;
  NameValuePairsParser parser(&map);
  const char* command[] = { "echo", "Foo" };
  EXPECT_TRUE(parser.GetSingleValueFromTool(arraysize(command), command,
                                            "foo"));
  ASSERT_EQ(1U, map.size());
  EXPECT_EQ("Foo", map["foo"]);
}

TEST(NameValuePairsParser, TestParseNameValuePairsFromTool) {
  NameValuePairsParser::NameValueMap map;
  NameValuePairsParser parser(&map);
  const char* command1[] = { "echo", "foo=Foo bar=Bar\nfoobar=FooBar\n" };
  EXPECT_TRUE(parser.ParseNameValuePairsFromTool(
      arraysize(command1), command1, "=", " \n"));
  ASSERT_EQ(3U, map.size());
  EXPECT_EQ("Foo", map["foo"]);
  EXPECT_EQ("Bar", map["bar"]);
  EXPECT_EQ("FooBar", map["foobar"]);

  map.clear();
  const char* command2[] = { "echo", "foo=Foo,bar=Bar" };
  EXPECT_TRUE(parser.ParseNameValuePairsFromTool(
      arraysize(command2), command2, "=", ",\n"));
  ASSERT_EQ(2U, map.size());
  EXPECT_EQ("Foo", map["foo"]);
  EXPECT_EQ("Bar", map["bar"]);

  map.clear();
  const char* command3[] = { "echo", "foo=Foo=foo,bar=Bar" };
  EXPECT_FALSE(parser.ParseNameValuePairsFromTool(
      arraysize(command3), command3, "=", ",\n"));

  map.clear();
  const char* command4[] = { "echo", "foo=Foo,=Bar" };
  EXPECT_FALSE(parser.ParseNameValuePairsFromTool(
      arraysize(command4), command4, "=", ",\n"));

  map.clear();
  const char* command5[] = { "echo",
      "\"initial_locale\"=\"ja\"\n"
      "\"initial_timezone\"=\"Asia/Tokyo\"\n"
      "\"keyboard_layout\"=\"mozc-jp\"\n" };
  EXPECT_TRUE(parser.ParseNameValuePairsFromTool(
      arraysize(command5), command5, "=", "\n"));
  ASSERT_EQ(3U, map.size());
  EXPECT_EQ("ja", map["initial_locale"]);
  EXPECT_EQ("Asia/Tokyo", map["initial_timezone"]);
  EXPECT_EQ("mozc-jp", map["keyboard_layout"]);
}

}  // namespace chromeos
