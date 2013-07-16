// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_change_observer_x11.h"

// Undefine X's macros used in gtest.
#undef Bool
#undef None

#include "testing/gtest/include/gtest/gtest.h"

typedef testing::Test DisplayChangeObserverX11Test;

namespace ash {
namespace internal {

TEST_F(DisplayChangeObserverX11Test, TestBlackListedDisplay) {
  EXPECT_TRUE(ShouldIgnoreSize(10, 10));
  EXPECT_TRUE(ShouldIgnoreSize(40, 30));
  EXPECT_TRUE(ShouldIgnoreSize(50, 40));
  EXPECT_TRUE(ShouldIgnoreSize(160, 90));
  EXPECT_TRUE(ShouldIgnoreSize(160, 100));

  EXPECT_FALSE(ShouldIgnoreSize(50, 60));
  EXPECT_FALSE(ShouldIgnoreSize(100, 70));
  EXPECT_FALSE(ShouldIgnoreSize(272, 181));
}

}  // namespace internal
}  // namespace ash
