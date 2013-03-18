// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/tile_priority.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

TEST(TilePriorityTest, TimeForBoundsToIntersectWithScroll) {
  const float inf = std::numeric_limits<float>::infinity();
  gfx::Rect target(0, 0, 800, 600);
  gfx::Rect current(100, 100, 100, 100);
  EXPECT_EQ(0, TilePriority::TimeForBoundsToIntersect(
      gfx::Rect(-200, 0, 100, 100), current, 1, target));
  EXPECT_EQ(0, TilePriority::TimeForBoundsToIntersect(
      gfx::Rect(-100, 0, 100, 100), current, 1, target));
  EXPECT_EQ(0, TilePriority::TimeForBoundsToIntersect(
      gfx::Rect(400, 400, 100, 100), current, 1, target));

  current = gfx::Rect(-300, -300, 100, 100);
  EXPECT_EQ(inf, TilePriority::TimeForBoundsToIntersect(
      gfx::Rect(0, 0, 100, 100), current, 1, target));
  EXPECT_EQ(inf, TilePriority::TimeForBoundsToIntersect(
      gfx::Rect(-200, -200, 100, 100), current, 1, target));
  EXPECT_EQ(2, TilePriority::TimeForBoundsToIntersect(
      gfx::Rect(-400, -400, 100, 100), current, 1, target));
}

TEST(TilePriorityTest, TimeForBoundsToIntersectWithScale) {
  const float inf = std::numeric_limits<float>::infinity();
  gfx::Rect target(0, 0, 800, 600);
  gfx::Rect current(100, 100, 100, 100);
  EXPECT_EQ(0, TilePriority::TimeForBoundsToIntersect(
      gfx::Rect(-200, 0, 200, 200), current, 1, target));
  EXPECT_EQ(0, TilePriority::TimeForBoundsToIntersect(
      gfx::Rect(-100, 0, 50, 50), current, 1, target));
  EXPECT_EQ(0, TilePriority::TimeForBoundsToIntersect(
      gfx::Rect(400, 400, 400, 400), current, 1, target));

  current = gfx::Rect(-300, -300, 100, 100);
  EXPECT_EQ(inf, TilePriority::TimeForBoundsToIntersect(
      gfx::Rect(-400, -400, 300, 300), current, 1, target));
  EXPECT_EQ(8, TilePriority::TimeForBoundsToIntersect(
      gfx::Rect(-275, -275, 50, 50), current, 1, target));
  EXPECT_EQ(1, TilePriority::TimeForBoundsToIntersect(
      gfx::Rect(-450, -450, 50, 50), current, 1, target));
}

TEST(TilePriorityTest, ManhattanDistanceBetweenRects) {
  EXPECT_EQ(0, TilePriority::manhattanDistance(
      gfx::RectF(0, 0, 400, 400), gfx::RectF(0, 0, 100, 100)));

  EXPECT_EQ(2, TilePriority::manhattanDistance(
      gfx::Rect(0, 0, 400, 400), gfx::Rect(-100, -100, 100, 100)));

  EXPECT_EQ(1, TilePriority::manhattanDistance(
      gfx::Rect(0, 0, 400, 400), gfx::Rect(0, -100, 100, 100)));

  EXPECT_EQ(202, TilePriority::manhattanDistance(
      gfx::Rect(0, 0, 100, 100), gfx::Rect(200, 200, 100, 100)));
}

}  // namespace
}  // namespace cc
