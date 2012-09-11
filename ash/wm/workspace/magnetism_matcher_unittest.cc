// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/magnetism_matcher.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace internal {

// Trivial test case verifying assertions on left edge.
TEST(MagnetismMatcherTest, TrivialLeft) {
  const int distance = MagnetismMatcher::kMagneticDistance;
  const gfx::Rect initial_bounds(20, 10, 50, 60);
  MagnetismMatcher matcher(initial_bounds);
  EXPECT_FALSE(matcher.AreEdgesObscured());
  MagnetismEdge edge = MAGNETISM_EDGE_TOP;
  EXPECT_FALSE(matcher.ShouldAttach(
                   gfx::Rect(initial_bounds.x() - distance - 10,
                             initial_bounds.y() - distance - 10, 2, 3), &edge));
  EXPECT_FALSE(matcher.AreEdgesObscured());
  EXPECT_TRUE(matcher.ShouldAttach(
                  gfx::Rect(initial_bounds.x() - 2, initial_bounds.y(), 1, 1),
                  &edge));
  EXPECT_EQ(MAGNETISM_EDGE_LEFT, edge);
}

// Verifies we don't match an obscured corner.
TEST(MagnetismMatcherTest, ObscureLeading) {
  const int distance = MagnetismMatcher::kMagneticDistance;
  const gfx::Rect initial_bounds(20, 10, 150, 160);
  MagnetismMatcher matcher(initial_bounds);
  MagnetismEdge edge = MAGNETISM_EDGE_TOP;
  // Overlap with the upper right corner.
  EXPECT_FALSE(matcher.ShouldAttach(
                   gfx::Rect(initial_bounds.right() - distance * 2,
                             initial_bounds.y() - distance - 2,
                             distance * 3,
                             (distance + 2) * 2), &edge));
  EXPECT_FALSE(matcher.AreEdgesObscured());
  // Verify doesn't match the following which is obscured by first.
  EXPECT_FALSE(matcher.ShouldAttach(
                   gfx::Rect(initial_bounds.right() + 1,
                             initial_bounds.y(),
                             distance,
                             5), &edge));
  // Should match the following which extends into non-overlapping region.
  EXPECT_TRUE(matcher.ShouldAttach(
                   gfx::Rect(initial_bounds.right() + 1,
                             initial_bounds.y() + 5,
                             distance,
                             15), &edge));
  EXPECT_EQ(MAGNETISM_EDGE_RIGHT, edge);
}

// Verifies obscuring one side doesn't obscure the other.
TEST(MagnetismMatcherTest, DontObscureOtherSide) {
  const int distance = MagnetismMatcher::kMagneticDistance;
  const gfx::Rect initial_bounds(20, 10, 150, 160);
  MagnetismMatcher matcher(initial_bounds);
  MagnetismEdge edge = MAGNETISM_EDGE_TOP;
  // Overlap with the left side.
  EXPECT_FALSE(matcher.ShouldAttach(
                   gfx::Rect(initial_bounds.x() - distance + 1,
                             initial_bounds.y() + 2,
                             distance * 2 + 2,
                             initial_bounds.height() + distance * 4), &edge));
  EXPECT_FALSE(matcher.AreEdgesObscured());
  // Should match the right side since it isn't obscured.
  EXPECT_TRUE(matcher.ShouldAttach(
                   gfx::Rect(initial_bounds.right() - 1,
                             initial_bounds.y() + distance + 1,
                             distance,
                             5), &edge));
  EXPECT_EQ(MAGNETISM_EDGE_RIGHT, edge);
}

// Verifies we don't match an obscured center.
TEST(MagnetismMatcherTest, ObscureCenter) {
  const int distance = MagnetismMatcher::kMagneticDistance;
  const gfx::Rect initial_bounds(20, 10, 150, 160);
  MagnetismMatcher matcher(initial_bounds);
  MagnetismEdge edge = MAGNETISM_EDGE_TOP;
  // Overlap with the center bottom edge.
  EXPECT_FALSE(matcher.ShouldAttach(
                   gfx::Rect(100, initial_bounds.bottom() - distance - 2,
                             20,
                             (distance + 2) * 2), &edge));
  EXPECT_FALSE(matcher.AreEdgesObscured());
  // Verify doesn't match the following which is obscured by first.
  EXPECT_FALSE(matcher.ShouldAttach(
                   gfx::Rect(110, initial_bounds.bottom() + 1,
                             10, 5), &edge));
  // Should match the following which extends into non-overlapping region.
  EXPECT_TRUE(matcher.ShouldAttach(
                  gfx::Rect(90,
                            initial_bounds.bottom() + 1,
                            10, 5), &edge));
  EXPECT_EQ(MAGNETISM_EDGE_BOTTOM, edge);
}

// Verifies we don't match an obscured trailing edge.
TEST(MagnetismMatcherTest, ObscureTrailing) {
  const int distance = MagnetismMatcher::kMagneticDistance;
  const gfx::Rect initial_bounds(20, 10, 150, 160);
  MagnetismMatcher matcher(initial_bounds);
  MagnetismEdge edge = MAGNETISM_EDGE_TOP;
  // Overlap with the trailing left edge.
  EXPECT_FALSE(matcher.ShouldAttach(
                   gfx::Rect(initial_bounds.x() - distance - 2,
                             150,
                             (distance + 2) * 2,
                             50), &edge));
  EXPECT_FALSE(matcher.AreEdgesObscured());
  // Verify doesn't match the following which is obscured by first.
  EXPECT_FALSE(matcher.ShouldAttach(
                   gfx::Rect(initial_bounds.x() - 4,
                             160, 3, 20), &edge));
  // Should match the following which extends into non-overlapping region.
  EXPECT_TRUE(matcher.ShouldAttach(
                   gfx::Rect(initial_bounds.x() - 4,
                             140, 3, 20), &edge));
  EXPECT_EQ(MAGNETISM_EDGE_LEFT, edge);
}

}  // namespace internal
}  // namespace ash

