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
  EXPECT_EQ(8u, SpdyDataFrame::size());
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

TEST_P(SpdyProtocolTest, DataFrameStructs) {
  SpdyDataFrame data_frame;
  data_frame.set_stream_id(12345);
  EXPECT_EQ(12345u, data_frame.stream_id());
}

TEST_P(SpdyProtocolTest, TestDataFrame) {
  SpdyDataFrame frame;

  // Set the stream ID to various values.
  frame.set_stream_id(0);
  EXPECT_EQ(0u, frame.stream_id());
  frame.set_stream_id(~0 & kStreamIdMask);
  EXPECT_EQ(~0 & kStreamIdMask, frame.stream_id());

  // Set length to various values.  Make sure that when you set_length(x),
  // length() == x.  Also make sure the flags are unaltered.
  memset(frame.data(), '1', SpdyDataFrame::size());
  int8 flags = frame.flags();
  frame.set_length(0);
  EXPECT_EQ(0u, frame.length());
  EXPECT_EQ(flags, frame.flags());
  frame.set_length(kLengthMask);
  EXPECT_EQ(kLengthMask, frame.length());
  EXPECT_EQ(flags, frame.flags());
  frame.set_length(5u);
  EXPECT_EQ(5u, frame.length());
  EXPECT_EQ(flags, frame.flags());

  // Set flags to various values.  Make sure that when you set_flags(x),
  // flags() == x.  Also make sure the length is unaltered.
  memset(frame.data(), '1', SpdyDataFrame::size());
  uint32 length = frame.length();
  frame.set_flags(0u);
  EXPECT_EQ(0u, frame.flags());
  EXPECT_EQ(length, frame.length());
  int8 all_flags = ~0;
  frame.set_flags(all_flags);
  flags = frame.flags();
  EXPECT_EQ(all_flags, flags);
  EXPECT_EQ(length, frame.length());
  frame.set_flags(5u);
  EXPECT_EQ(5u, frame.flags());
  EXPECT_EQ(length, frame.length());
}

class SpdyProtocolDeathTest : public SpdyProtocolTest {};

// All tests are run with two different SPDY versions: SPDY/2 and SPDY/3.
INSTANTIATE_TEST_CASE_P(SpdyProtocolDeathTests,
                        SpdyProtocolDeathTest,
                        ::testing::Values(SPDY2, SPDY3));

// Make sure that overflows both die in debug mode, and do not cause problems
// in opt mode.  Note:  The EXPECT_DEBUG_DEATH call does not work on Win32 yet,
// so we comment it out.
TEST_P(SpdyProtocolDeathTest, TestDataFrame) {
  SpdyDataFrame frame;

  frame.set_stream_id(0);
  // TODO(mbelshe):  implement EXPECT_DEBUG_DEATH on windows.
#if !defined(WIN32) && defined(GTEST_HAS_DEATH_TEST)
#if !defined(DCHECK_ALWAYS_ON)
  EXPECT_DEBUG_DEATH(frame.set_stream_id(~0), "");
#else
  EXPECT_DEATH(frame.set_stream_id(~0), "");
#endif
#endif

  frame.set_flags(0);
#if !defined(WIN32) && defined(GTEST_HAS_DEATH_TEST)
#if !defined(DCHECK_ALWAYS_ON)
  EXPECT_DEBUG_DEATH(frame.set_length(~0), "");
#else
  EXPECT_DEATH(frame.set_length(~0), "");
#endif
#endif
  EXPECT_EQ(0, frame.flags());
}

}  // namespace net
