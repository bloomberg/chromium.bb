// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ssl_false_start_blacklist.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(SSLFalseStartBlacklistTest, LastTwoLabels) {
#define F net::SSLFalseStartBlacklist::LastTwoLabels
  EXPECT_STREQ(F("a.b.c.d"), "c.d");
  EXPECT_STREQ(F("a.b"), "a.b");
  EXPECT_STREQ(F("example.com"), "example.com");
  EXPECT_STREQ(F("www.example.com"), "example.com");
  EXPECT_STREQ(F("www.www.example.com"), "example.com");

  EXPECT_TRUE(F("com") == NULL);
  EXPECT_TRUE(F(".com") == NULL);
  EXPECT_TRUE(F("") == NULL);
#undef F
}

TEST(SSLFalseStartBlacklistTest, IsMember) {
  EXPECT_TRUE(net::SSLFalseStartBlacklist::IsMember("example.com"));
  EXPECT_TRUE(net::SSLFalseStartBlacklist::IsMember("www.example.com"));
  EXPECT_TRUE(net::SSLFalseStartBlacklist::IsMember("a.b.example.com"));
  EXPECT_FALSE(net::SSLFalseStartBlacklist::IsMember("aexample.com"));
  EXPECT_FALSE(net::SSLFalseStartBlacklist::IsMember("com"));
}
