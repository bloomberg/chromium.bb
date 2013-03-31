// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_utils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {
namespace {

TEST(QuicUtilsTest, StreamErrorToString) {
  EXPECT_STREQ("QUIC_BAD_APPLICATION_PAYLOAD",
               QuicUtils::StreamErrorToString(QUIC_BAD_APPLICATION_PAYLOAD));
}

TEST(QuicUtilsTest, ErrorToString) {
  EXPECT_STREQ("QUIC_NO_ERROR",
               QuicUtils::ErrorToString(QUIC_NO_ERROR));
}

}  // namespace
}  // namespace test
}  // namespace net
