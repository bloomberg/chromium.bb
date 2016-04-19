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

}  // namespace net
