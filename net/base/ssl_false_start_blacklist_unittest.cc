// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ssl_false_start_blacklist.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(SSLFalseStartBlacklistTest, LastTwoComponents) {
  EXPECT_EQ(SSLFalseStartBlacklist::LastTwoComponents("a.b.c.d"), "c.d");
  EXPECT_EQ(SSLFalseStartBlacklist::LastTwoComponents("a.b"), "a.b");
  EXPECT_EQ(SSLFalseStartBlacklist::LastTwoComponents("www.a.de"), "a.de");
  EXPECT_EQ(SSLFalseStartBlacklist::LastTwoComponents("www.www.a.de"), "a.de");
  EXPECT_EQ(SSLFalseStartBlacklist::LastTwoComponents("a.com."), "a.com");
  EXPECT_EQ(SSLFalseStartBlacklist::LastTwoComponents("a.com.."), "a.com");

  EXPECT_TRUE(SSLFalseStartBlacklist::LastTwoComponents("com").empty());
  EXPECT_TRUE(SSLFalseStartBlacklist::LastTwoComponents(".com").empty());
  EXPECT_TRUE(SSLFalseStartBlacklist::LastTwoComponents("").empty());
}

TEST(SSLFalseStartBlacklistTest, IsMember) {
  EXPECT_TRUE(SSLFalseStartBlacklist::IsMember("example.com"));
  EXPECT_TRUE(SSLFalseStartBlacklist::IsMember("www.example.com"));
  EXPECT_TRUE(SSLFalseStartBlacklist::IsMember("a.b.example.com"));
  EXPECT_FALSE(SSLFalseStartBlacklist::IsMember("aexample.com"));
  EXPECT_FALSE(SSLFalseStartBlacklist::IsMember("com"));

  EXPECT_TRUE(SSLFalseStartBlacklist::IsMember("www.toto-dream.com"));
}

}  // namespace net
