// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_protocol.h"

#include <limits>
#include <memory>

#include "net/spdy/spdy_bitmasks.h"
#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_test_utils.h"
#include "net/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

}  // namespace

namespace net {

TEST(SpdyProtocolDeathTest, TestSpdySettingsAndIdOutOfBounds) {
  std::unique_ptr<SettingsFlagsAndId> flags_and_id;

  EXPECT_SPDY_BUG(flags_and_id.reset(new SettingsFlagsAndId(1, 0xffffffff)),
                  "SPDY setting ID too large.");
  // Make sure that we get expected values in opt mode.
  if (flags_and_id.get() != nullptr) {
    EXPECT_EQ(1, flags_and_id->flags());
    EXPECT_EQ(0xffffffu, flags_and_id->id());
  }
}

TEST(SpdyProtocolTest, IsValidHTTP2FrameStreamId) {
  // Stream-specific frames must have non-zero stream ids
  EXPECT_TRUE(SpdyConstants::IsValidHTTP2FrameStreamId(1, DATA));
  EXPECT_FALSE(SpdyConstants::IsValidHTTP2FrameStreamId(0, DATA));
  EXPECT_TRUE(SpdyConstants::IsValidHTTP2FrameStreamId(1, HEADERS));
  EXPECT_FALSE(SpdyConstants::IsValidHTTP2FrameStreamId(0, HEADERS));
  EXPECT_TRUE(SpdyConstants::IsValidHTTP2FrameStreamId(1, PRIORITY));
  EXPECT_FALSE(SpdyConstants::IsValidHTTP2FrameStreamId(0, PRIORITY));
  EXPECT_TRUE(SpdyConstants::IsValidHTTP2FrameStreamId(1, RST_STREAM));
  EXPECT_FALSE(SpdyConstants::IsValidHTTP2FrameStreamId(0, RST_STREAM));
  EXPECT_TRUE(SpdyConstants::IsValidHTTP2FrameStreamId(1, CONTINUATION));
  EXPECT_FALSE(SpdyConstants::IsValidHTTP2FrameStreamId(0, CONTINUATION));
  EXPECT_TRUE(SpdyConstants::IsValidHTTP2FrameStreamId(1, PUSH_PROMISE));
  EXPECT_FALSE(SpdyConstants::IsValidHTTP2FrameStreamId(0, PUSH_PROMISE));

  // Connection-level frames must have zero stream ids
  EXPECT_FALSE(SpdyConstants::IsValidHTTP2FrameStreamId(1, GOAWAY));
  EXPECT_TRUE(SpdyConstants::IsValidHTTP2FrameStreamId(0, GOAWAY));
  EXPECT_FALSE(SpdyConstants::IsValidHTTP2FrameStreamId(1, SETTINGS));
  EXPECT_TRUE(SpdyConstants::IsValidHTTP2FrameStreamId(0, SETTINGS));
  EXPECT_FALSE(SpdyConstants::IsValidHTTP2FrameStreamId(1, PING));
  EXPECT_TRUE(SpdyConstants::IsValidHTTP2FrameStreamId(0, PING));

  // Frames that are neither stream-specific nor connection-level
  // should not have their stream id declared invalid
  EXPECT_TRUE(SpdyConstants::IsValidHTTP2FrameStreamId(1, WINDOW_UPDATE));
  EXPECT_TRUE(SpdyConstants::IsValidHTTP2FrameStreamId(0, WINDOW_UPDATE));
}

}  // namespace net
