// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system_access.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace system_access {

TEST(SystemAccessTest, TestGetSingleValueFromTool) {
  ChromeOSSystem system;
  EXPECT_TRUE(system.GetSingleValueFromTool(
      "echo Foo", "foo"));
  EXPECT_STREQ("foo", system.nv_pairs()[0].first.c_str());
  EXPECT_STREQ("Foo", system.nv_pairs()[0].second.c_str());
}

TEST(SystemAccessTest, TestParseNVPairsFromTool) {
  ChromeOSSystem system;
  EXPECT_TRUE(system.ParseNVPairsFromTool(
      "echo 'foo=Foo bar=Bar\nfoobar=FooBar\n'", "=", " \n"));
  EXPECT_STREQ("foo", system.nv_pairs()[0].first.c_str());
  EXPECT_STREQ("Foo", system.nv_pairs()[0].second.c_str());
  EXPECT_STREQ("bar", system.nv_pairs()[1].first.c_str());
  EXPECT_STREQ("Bar", system.nv_pairs()[1].second.c_str());
  EXPECT_STREQ("foobar", system.nv_pairs()[2].first.c_str());
  EXPECT_STREQ("FooBar", system.nv_pairs()[2].second.c_str());
  EXPECT_EQ(3U, system.nv_pairs().size());

  EXPECT_TRUE(system.ParseNVPairsFromTool(
      "echo 'foo=Foo,bar=Bar'", "=", ",\n"));
  EXPECT_STREQ("foo", system.nv_pairs()[0].first.c_str());
  EXPECT_STREQ("Foo", system.nv_pairs()[0].second.c_str());
  EXPECT_STREQ("bar", system.nv_pairs()[1].first.c_str());
  EXPECT_STREQ("Bar", system.nv_pairs()[1].second.c_str());

  EXPECT_FALSE(system.ParseNVPairsFromTool(
      "echo 'foo=Foo=foo,bar=Bar'", "=", ",\n"));

  EXPECT_FALSE(system.ParseNVPairsFromTool(
      "echo 'foo=Foo,=Bar'", "=", ",\n"));
}

}  // namespace system_access
}  // namespace chromeos
