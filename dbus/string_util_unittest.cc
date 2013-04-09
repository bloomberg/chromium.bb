// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(StringUtilTest, IsValidObjectPath) {
  EXPECT_TRUE(dbus::IsValidObjectPath("/"));
  EXPECT_TRUE(dbus::IsValidObjectPath("/foo/bar"));
  EXPECT_TRUE(dbus::IsValidObjectPath("/hoge_fuga/piyo123"));
  // Empty string.
  EXPECT_FALSE(dbus::IsValidObjectPath(std::string()));
  // Emptyr elemnt.
  EXPECT_FALSE(dbus::IsValidObjectPath("//"));
  EXPECT_FALSE(dbus::IsValidObjectPath("/foo//bar"));
  EXPECT_FALSE(dbus::IsValidObjectPath("/foo///bar"));
  // Trailing '/'.
  EXPECT_FALSE(dbus::IsValidObjectPath("/foo/"));
  EXPECT_FALSE(dbus::IsValidObjectPath("/foo/bar/"));
  // Not beginning with '/'.
  EXPECT_FALSE(dbus::IsValidObjectPath("foo/bar"));
  // Invalid characters.
  EXPECT_FALSE(dbus::IsValidObjectPath("/foo.bar"));
  EXPECT_FALSE(dbus::IsValidObjectPath("/foo/*"));
  EXPECT_FALSE(dbus::IsValidObjectPath("/foo/bar(1)"));
}
