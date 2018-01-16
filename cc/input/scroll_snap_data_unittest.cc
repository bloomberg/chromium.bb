// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/scroll_snap_data.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

class ScrollSnapDataTest : public testing::Test {};

TEST_F(ScrollSnapDataTest, FindsClosestSnapPositionIndependently) {
  SnapContainerData data(
      ScrollSnapType(false, SnapAxis::kBoth, SnapStrictness::kMandatory),
      gfx::ScrollOffset(360, 380));
  gfx::ScrollOffset current_position(100, 100);
  SnapAreaData snap_x_only(
      SnapAxis::kX, gfx::ScrollOffset(80, SnapAreaData::kInvalidScrollPosition),
      false);
  SnapAreaData snap_y_only(
      SnapAxis::kY, gfx::ScrollOffset(SnapAreaData::kInvalidScrollPosition, 70),
      false);
  SnapAreaData snap_on_both(SnapAxis::kBoth, gfx::ScrollOffset(50, 150), false);
  data.AddSnapAreaData(snap_x_only);
  data.AddSnapAreaData(snap_y_only);
  data.AddSnapAreaData(snap_on_both);
  gfx::ScrollOffset snap_position =
      data.FindSnapPosition(current_position, true, true);
  EXPECT_EQ(80, snap_position.x());
  EXPECT_EQ(70, snap_position.y());
}

TEST_F(ScrollSnapDataTest, FindsClosestSnapPositionOnAxisValueBoth) {
  SnapContainerData data(
      ScrollSnapType(false, SnapAxis::kBoth, SnapStrictness::kMandatory),
      gfx::ScrollOffset(360, 380));
  gfx::ScrollOffset current_position(40, 150);
  SnapAreaData snap_x_only(
      SnapAxis::kX, gfx::ScrollOffset(80, SnapAreaData::kInvalidScrollPosition),
      false);
  SnapAreaData snap_y_only(
      SnapAxis::kY, gfx::ScrollOffset(SnapAreaData::kInvalidScrollPosition, 70),
      false);
  SnapAreaData snap_on_both(SnapAxis::kBoth, gfx::ScrollOffset(50, 150), false);
  data.AddSnapAreaData(snap_x_only);
  data.AddSnapAreaData(snap_y_only);
  data.AddSnapAreaData(snap_on_both);
  gfx::ScrollOffset snap_position =
      data.FindSnapPosition(current_position, true, true);
  EXPECT_EQ(50, snap_position.x());
  EXPECT_EQ(150, snap_position.y());
}

TEST_F(ScrollSnapDataTest, DoesNotSnapOnNonScrolledAxis) {
  SnapContainerData data(
      ScrollSnapType(false, SnapAxis::kBoth, SnapStrictness::kMandatory),
      gfx::ScrollOffset(360, 380));
  gfx::ScrollOffset current_position(100, 100);
  SnapAreaData snap_x_only(
      SnapAxis::kX, gfx::ScrollOffset(80, SnapAreaData::kInvalidScrollPosition),
      false);
  SnapAreaData snap_y_only(
      SnapAxis::kY, gfx::ScrollOffset(SnapAreaData::kInvalidScrollPosition, 70),
      false);
  data.AddSnapAreaData(snap_x_only);
  data.AddSnapAreaData(snap_y_only);
  gfx::ScrollOffset snap_position =
      data.FindSnapPosition(current_position, true, false);
  EXPECT_EQ(80, snap_position.x());
  EXPECT_EQ(100, snap_position.y());
}

}  // namespace cc
