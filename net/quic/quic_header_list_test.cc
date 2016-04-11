// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_header_list.h"

#include "net/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace net {

class QuicHeaderListTest : public ::testing::Test {};

// This test verifies that QuicHeaderList accumulates header pairs in order.
TEST(QuicHeaderListTest, OnHeader) {
  QuicHeaderList headers;
  headers.OnHeader("foo", "bar");
  headers.OnHeader("april", "fools");
  headers.OnHeader("beep", "");

  std::string accumulator;
  for (const auto& p : headers) {
    accumulator.append("(" + p.first + ", " + p.second + ") ");
  }
  EXPECT_EQ("(foo, bar) (april, fools) (beep, ) ", accumulator);
}

// This test verifies that QuicHeaderList is copyable and assignable.
TEST(QuicHeaderListTest, IsCopyableAndAssignable) {
  QuicHeaderList headers;
  headers.OnHeader("foo", "bar");
  headers.OnHeader("april", "fools");
  headers.OnHeader("beep", "");

  QuicHeaderList headers2(headers);
  QuicHeaderList headers3 = headers;

  std::string accumulator;
  for (const auto& p : headers2) {
    accumulator.append("(" + p.first + ", " + p.second + ") ");
  }
  EXPECT_EQ("(foo, bar) (april, fools) (beep, ) ", accumulator);

  accumulator.clear();
  for (const auto& p : headers3) {
    accumulator.append("(" + p.first + ", " + p.second + ") ");
  }
  EXPECT_EQ("(foo, bar) (april, fools) (beep, ) ", accumulator);
}

}  // namespace net
