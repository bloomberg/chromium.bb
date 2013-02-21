// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_protocol.h"

#include "base/memory/scoped_ptr.h"
#include "net/spdy/spdy_bitmasks.h"
#include "net/spdy/spdy_framer.h"
#include "testing/platform_test.h"

namespace {

enum SpdyProtocolTestTypes {
  SPDY2 = 2,
  SPDY3 = 3,
};

}  // namespace

namespace net {

class SpdyProtocolTest
    : public ::testing::TestWithParam<SpdyProtocolTestTypes> {
 protected:
  virtual void SetUp() {
    spdy_version_ = GetParam();
  }

  bool IsSpdy2() { return spdy_version_ == SPDY2; }

  // Version of SPDY protocol to be used.
  int spdy_version_;
};

// All tests are run with two different SPDY versions: SPDY/2 and SPDY/3.
INSTANTIATE_TEST_CASE_P(SpdyProtocolTests,
                        SpdyProtocolTest,
                        ::testing::Values(SPDY2, SPDY3));

// Test our protocol constants
TEST_P(SpdyProtocolTest, ProtocolConstants) {
  EXPECT_EQ(8u, SpdyFrame::kHeaderSize);
  EXPECT_EQ(4u, sizeof(FlagsAndLength));
  EXPECT_EQ(1, SYN_STREAM);
  EXPECT_EQ(2, SYN_REPLY);
  EXPECT_EQ(3, RST_STREAM);
  EXPECT_EQ(4, SETTINGS);
  EXPECT_EQ(5, NOOP);
  EXPECT_EQ(6, PING);
  EXPECT_EQ(7, GOAWAY);
  EXPECT_EQ(8, HEADERS);
  EXPECT_EQ(9, WINDOW_UPDATE);
}

// Test some of the protocol helper functions
TEST_P(SpdyProtocolTest, FrameStructs) {
  SpdyFrame frame(SpdyFrame::kHeaderSize);
  frame.set_length(12345);
  frame.set_flags(10);
  EXPECT_EQ(12345u, frame.length());
  EXPECT_EQ(10u, frame.flags());

  frame.set_length(0);
  frame.set_flags(10);
  EXPECT_EQ(0u, frame.length());
  EXPECT_EQ(10u, frame.flags());
}

class SpdyProtocolDeathTest : public SpdyProtocolTest {};

// All tests are run with two different SPDY versions: SPDY/2 and SPDY/3.
INSTANTIATE_TEST_CASE_P(SpdyProtocolDeathTests,
                        SpdyProtocolDeathTest,
                        ::testing::Values(SPDY2, SPDY3));

}  // namespace net
