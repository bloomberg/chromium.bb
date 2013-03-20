// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_protocol.h"

#include <limits>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "net/spdy/spdy_bitmasks.h"
#include "net/spdy/spdy_framer.h"
#include "testing/gtest/include/gtest/gtest.h"

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
  EXPECT_EQ(1, SYN_STREAM);
  EXPECT_EQ(2, SYN_REPLY);
  EXPECT_EQ(3, RST_STREAM);
  EXPECT_EQ(4, SETTINGS);
  EXPECT_EQ(5, NOOP);
  EXPECT_EQ(6, PING);
  EXPECT_EQ(7, GOAWAY);
  EXPECT_EQ(8, HEADERS);
  EXPECT_EQ(9, WINDOW_UPDATE);
  EXPECT_EQ(std::numeric_limits<int32>::max(), kSpdyMaximumWindowSize);
}

class SpdyProtocolDeathTest : public SpdyProtocolTest {};

// All tests are run with two different SPDY versions: SPDY/2 and SPDY/3.
INSTANTIATE_TEST_CASE_P(SpdyProtocolDeathTests,
                        SpdyProtocolDeathTest,
                        ::testing::Values(SPDY2, SPDY3));

#if GTEST_HAS_DEATH_TEST && !defined(NDEBUG)
TEST_P(SpdyProtocolDeathTest, TestSpdySettingsAndIdOutOfBounds) {
  scoped_ptr<SettingsFlagsAndId> flags_and_id;

  EXPECT_DEBUG_DEATH(
      {
        flags_and_id.reset(new SettingsFlagsAndId(1, ~0));
      },
      "SPDY setting ID too large.");
  // Make sure that we get expected values in opt mode.
  if (flags_and_id.get() != NULL) {
    EXPECT_EQ(1, flags_and_id->flags());
    EXPECT_EQ(static_cast<SpdyPingId>(0xffffff), flags_and_id->id());
  }
}
#endif  // GTEST_HAS_DEATH_TEST && !defined(NDEBUG)

}  // namespace net
