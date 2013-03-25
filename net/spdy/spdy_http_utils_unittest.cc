// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_http_utils.h"

#include "base/basictypes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

TEST(SpdyHttpUtilsTest, ConvertRequestPriorityToSpdy2Priority) {
  EXPECT_EQ(0, ConvertRequestPriorityToSpdyPriority(HIGHEST, 2));
  EXPECT_EQ(1, ConvertRequestPriorityToSpdyPriority(MEDIUM, 2));
  EXPECT_EQ(2, ConvertRequestPriorityToSpdyPriority(LOW, 2));
  EXPECT_EQ(2, ConvertRequestPriorityToSpdyPriority(LOWEST, 2));
  EXPECT_EQ(3, ConvertRequestPriorityToSpdyPriority(IDLE, 2));
}

TEST(SpdyHttpUtilsTest, ConvertRequestPriorityToSpdy3Priority) {
  EXPECT_EQ(0, ConvertRequestPriorityToSpdyPriority(HIGHEST, 3));
  EXPECT_EQ(1, ConvertRequestPriorityToSpdyPriority(MEDIUM, 3));
  EXPECT_EQ(2, ConvertRequestPriorityToSpdyPriority(LOW, 3));
  EXPECT_EQ(3, ConvertRequestPriorityToSpdyPriority(LOWEST, 3));
  EXPECT_EQ(4, ConvertRequestPriorityToSpdyPriority(IDLE, 3));
}

TEST(SpdyHttpUtilsTest, ConvertSpdy2PriorityToRequestPriority) {
  EXPECT_EQ(HIGHEST, ConvertSpdyPriorityToRequestPriority(0, 2));
  EXPECT_EQ(MEDIUM, ConvertSpdyPriorityToRequestPriority(1, 2));
  EXPECT_EQ(LOW, ConvertSpdyPriorityToRequestPriority(2, 2));
  EXPECT_EQ(IDLE, ConvertSpdyPriorityToRequestPriority(3, 2));
  // These are invalid values, but we should still handle them
  // gracefully.
  for (int i = 4; i < kuint8max; ++i) {
    EXPECT_EQ(IDLE, ConvertSpdyPriorityToRequestPriority(i, 2));
  }
}

TEST(SpdyHttpUtilsTest, ConvertSpdy3PriorityToRequestPriority) {
  EXPECT_EQ(HIGHEST, ConvertSpdyPriorityToRequestPriority(0, 3));
  EXPECT_EQ(MEDIUM, ConvertSpdyPriorityToRequestPriority(1, 3));
  EXPECT_EQ(LOW, ConvertSpdyPriorityToRequestPriority(2, 3));
  EXPECT_EQ(LOWEST, ConvertSpdyPriorityToRequestPriority(3, 3));
  EXPECT_EQ(IDLE, ConvertSpdyPriorityToRequestPriority(4, 3));
  // These are invalid values, but we should still handle them
  // gracefully.
  for (int i = 5; i < kuint8max; ++i) {
    EXPECT_EQ(IDLE, ConvertSpdyPriorityToRequestPriority(i, 3));
  }
}

TEST(SpdyHttpUtilsTest, ShowHttpHeaderValue){
#if defined(SPDY_PROXY_AUTH_ORIGIN)
  EXPECT_FALSE(ShouldShowHttpHeaderValue("proxy-authorization"));
  EXPECT_TRUE(ShouldShowHttpHeaderValue("accept-encoding"));
#else
  EXPECT_TRUE(ShouldShowHttpHeaderValue("proxy-authorization"));
  EXPECT_TRUE(ShouldShowHttpHeaderValue("accept-encoding"));
#endif
}

}  // namespace

}  // namespace net
