// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system_access.h"

#include "base/basictypes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace system_access {

TEST(SystemAccessTest, TestGetSingleValueFromTool) {
  MachineInfo machine_info;
  NameValuePairsParser parser(&machine_info);
  const char* command[] = { "echo", "Foo" };
  EXPECT_TRUE(parser.GetSingleValueFromTool(arraysize(command), command,
                                            "foo"));
  ASSERT_EQ(1U, machine_info.size());
  EXPECT_EQ("Foo", machine_info["foo"]);
}

TEST(SystemAccessTest, TestParseNameValuePairsFromTool) {
  MachineInfo machine_info;
  NameValuePairsParser parser(&machine_info);
  const char* command1[] = { "echo", "foo=Foo bar=Bar\nfoobar=FooBar\n" };
  EXPECT_TRUE(parser.ParseNameValuePairsFromTool(
      arraysize(command1), command1, "=", " \n"));
  ASSERT_EQ(3U, machine_info.size());
  EXPECT_EQ("Foo", machine_info["foo"]);
  EXPECT_EQ("Bar", machine_info["bar"]);
  EXPECT_EQ("FooBar", machine_info["foobar"]);

  machine_info.clear();
  const char* command2[] = { "echo", "foo=Foo,bar=Bar" };
  EXPECT_TRUE(parser.ParseNameValuePairsFromTool(
      arraysize(command2), command2, "=", ",\n"));
  ASSERT_EQ(2U, machine_info.size());
  EXPECT_EQ("Foo", machine_info["foo"]);
  EXPECT_EQ("Bar", machine_info["bar"]);

  machine_info.clear();
  const char* command3[] = { "echo", "foo=Foo=foo,bar=Bar" };
  EXPECT_FALSE(parser.ParseNameValuePairsFromTool(
      arraysize(command3), command3, "=", ",\n"));

  machine_info.clear();
  const char* command4[] = { "echo", "foo=Foo,=Bar" };
  EXPECT_FALSE(parser.ParseNameValuePairsFromTool(
      arraysize(command4), command4, "=", ",\n"));
}

}  // namespace system_access
}  // namespace chromeos
