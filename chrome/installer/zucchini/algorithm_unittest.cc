// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/zucchini/algorithm.h"

#include <stddef.h>
#include <stdint.h>

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

TEST(Algorithm, RangeIsBounded) {
  // Basic tests.
  EXPECT_TRUE(RangeIsBounded<uint8_t>(0U, +0U, 10U));
  EXPECT_TRUE(RangeIsBounded<uint8_t>(0U, +10U, 10U));
  EXPECT_TRUE(RangeIsBounded<uint8_t>(1U, +9U, 10U));
  EXPECT_FALSE(RangeIsBounded<uint8_t>(1U, +10U, 10U));
  EXPECT_TRUE(RangeIsBounded<uint8_t>(8U, +1U, 10U));
  EXPECT_TRUE(RangeIsBounded<uint8_t>(8U, +2U, 10U));
  EXPECT_TRUE(RangeIsBounded<uint8_t>(9U, +0U, 10U));
  EXPECT_FALSE(RangeIsBounded<uint8_t>(10U, +0U, 10U));  // !
  EXPECT_FALSE(RangeIsBounded<uint8_t>(100U, +0U, 10U));
  EXPECT_FALSE(RangeIsBounded<uint8_t>(100U, +1U, 10U));

  // Test at boundary of overflow.
  EXPECT_TRUE(RangeIsBounded<uint8_t>(42U, +137U, 255U));
  EXPECT_TRUE(RangeIsBounded<uint8_t>(0U, +255U, 255U));
  EXPECT_TRUE(RangeIsBounded<uint8_t>(1U, +254U, 255U));
  EXPECT_FALSE(RangeIsBounded<uint8_t>(1U, +255U, 255U));
  EXPECT_TRUE(RangeIsBounded<uint8_t>(254U, +0U, 255U));
  EXPECT_TRUE(RangeIsBounded<uint8_t>(254U, +1U, 255U));
  EXPECT_FALSE(RangeIsBounded<uint8_t>(255U, +0U, 255U));
  EXPECT_FALSE(RangeIsBounded<uint8_t>(255U, +3U, 255U));

  // Test with uint32_t.
  EXPECT_TRUE(RangeIsBounded<uint32_t>(0U, +0x1000U, 0x2000U));
  EXPECT_TRUE(RangeIsBounded<uint32_t>(0x0FFFU, +0x1000U, 0x2000U));
  EXPECT_TRUE(RangeIsBounded<uint32_t>(0x1000U, +0x1000U, 0x2000U));
  EXPECT_FALSE(RangeIsBounded<uint32_t>(0x1000U, +0x1001U, 0x2000U));
  EXPECT_TRUE(RangeIsBounded<uint32_t>(0x1FFFU, +1U, 0x2000U));
  EXPECT_FALSE(RangeIsBounded<uint32_t>(0x2000U, +0U, 0x2000U));  // !
  EXPECT_FALSE(RangeIsBounded<uint32_t>(0x3000U, +0U, 0x2000U));
  EXPECT_FALSE(RangeIsBounded<uint32_t>(0x3000U, +1U, 0x2000U));
  EXPECT_TRUE(RangeIsBounded<uint32_t>(0U, +0xFFFFFFFEU, 0xFFFFFFFFU));
  EXPECT_TRUE(RangeIsBounded<uint32_t>(0U, +0xFFFFFFFFU, 0xFFFFFFFFU));
  EXPECT_TRUE(RangeIsBounded<uint32_t>(1U, +0xFFFFFFFEU, 0xFFFFFFFFU));
  EXPECT_FALSE(RangeIsBounded<uint32_t>(1U, +0xFFFFFFFFU, 0xFFFFFFFFU));
  EXPECT_TRUE(RangeIsBounded<uint32_t>(0x80000000U, +0x7FFFFFFFU, 0xFFFFFFFFU));
  EXPECT_FALSE(
      RangeIsBounded<uint32_t>(0x80000000U, +0x80000000U, 0xFFFFFFFFU));
  EXPECT_TRUE(RangeIsBounded<uint32_t>(0xFFFFFFFEU, +1U, 0xFFFFFFFFU));
  EXPECT_FALSE(RangeIsBounded<uint32_t>(0xFFFFFFFFU, +0U, 0xFFFFFFFFU));  // !
  EXPECT_FALSE(
      RangeIsBounded<uint32_t>(0xFFFFFFFFU, +0xFFFFFFFFU, 0xFFFFFFFFU));
}

}  // namespace zucchini
