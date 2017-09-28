// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/frame_sinks/copy_output_util.h"

#include <limits>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"

namespace viz {
namespace copy_output {

namespace {

// Some very large integers that should be big enough to trigger overflow
// conditions, if the implementation is doing bad math.
constexpr int kMaxInt = std::numeric_limits<int>::max();
constexpr int kMaxEvenInt = kMaxInt - 1;

// These are all equivalent to a scale ratio of 1:1.
constexpr gfx::Vector2d kNonScalingVectors[5][2] = {
    {gfx::Vector2d(1, 1), gfx::Vector2d(1, 1)},
    {gfx::Vector2d(42, 1), gfx::Vector2d(42, 1)},
    {gfx::Vector2d(1, 42), gfx::Vector2d(1, 42)},
    {gfx::Vector2d(kMaxInt, 1), gfx::Vector2d(kMaxInt, 1)},
    {gfx::Vector2d(1, kMaxInt), gfx::Vector2d(1, kMaxInt)},
};

// These are all equivalent to a scale ratio of 1:2 in the X direction.
constexpr gfx::Vector2d kDoublingXVectors[4][2] = {
    {gfx::Vector2d(1, 1), gfx::Vector2d(2, 1)},
    {gfx::Vector2d(21, 1), gfx::Vector2d(42, 1)},
    {gfx::Vector2d(kMaxEvenInt / 2, 1), gfx::Vector2d(kMaxEvenInt, 1)},
    {gfx::Vector2d(kMaxEvenInt / 2, kMaxInt),
     gfx::Vector2d(kMaxEvenInt, kMaxInt)},
};

// These are all equivalent to a scale ratio of 1:2 in the Y direction.
constexpr gfx::Vector2d kDoublingYVectors[4][2] = {
    {gfx::Vector2d(1, 1), gfx::Vector2d(1, 2)},
    {gfx::Vector2d(1, 55), gfx::Vector2d(1, 110)},
    {gfx::Vector2d(1, kMaxEvenInt / 2), gfx::Vector2d(1, kMaxEvenInt)},
    {gfx::Vector2d(kMaxInt, kMaxEvenInt / 2),
     gfx::Vector2d(kMaxInt, kMaxEvenInt)},
};

TEST(CopyOutputUtil, ComputesValidResultRects) {
  for (const gfx::Vector2d* v : kNonScalingVectors) {
    SCOPED_TRACE(::testing::Message()
                 << "v[0]=" << v[0].ToString() << ", v[1]=" << v[1].ToString());
    EXPECT_EQ(gfx::Rect(1, 2, 3, 4),
              ComputeResultRect(gfx::Rect(1, 2, 3, 4), v[0], v[1]));
    EXPECT_EQ(gfx::Rect(-1, 2, 3, 4),
              ComputeResultRect(gfx::Rect(-1, 2, 3, 4), v[0], v[1]));
    EXPECT_EQ(gfx::Rect(1, -2, 3, 4),
              ComputeResultRect(gfx::Rect(1, -2, 3, 4), v[0], v[1]));
  }
  for (const gfx::Vector2d* v : kDoublingXVectors) {
    SCOPED_TRACE(::testing::Message()
                 << "v[0]=" << v[0].ToString() << ", v[1]=" << v[1].ToString());
    EXPECT_EQ(gfx::Rect(2, 2, 6, 4),
              ComputeResultRect(gfx::Rect(1, 2, 3, 4), v[0], v[1]));
    EXPECT_EQ(gfx::Rect(-2, 2, 6, 4),
              ComputeResultRect(gfx::Rect(-1, 2, 3, 4), v[0], v[1]));
    EXPECT_EQ(gfx::Rect(2, -2, 6, 4),
              ComputeResultRect(gfx::Rect(1, -2, 3, 4), v[0], v[1]));
  }
  for (const gfx::Vector2d* v : kDoublingYVectors) {
    SCOPED_TRACE(::testing::Message()
                 << "v[0]=" << v[0].ToString() << ", v[1]=" << v[1].ToString());
    EXPECT_EQ(gfx::Rect(1, 4, 3, 8),
              ComputeResultRect(gfx::Rect(1, 2, 3, 4), v[0], v[1]));
    EXPECT_EQ(gfx::Rect(-1, 4, 3, 8),
              ComputeResultRect(gfx::Rect(-1, 2, 3, 4), v[0], v[1]));
    EXPECT_EQ(gfx::Rect(1, -4, 3, 8),
              ComputeResultRect(gfx::Rect(1, -2, 3, 4), v[0], v[1]));
  }

  // Scale 3:2 in the X direction and 7:3 in the Y direction.
  constexpr gfx::Vector2d kWeirdScaleFrom = gfx::Vector2d(3, 7);
  constexpr gfx::Vector2d kWeirdScaleTo = gfx::Vector2d(2, 3);
  EXPECT_EQ(
      gfx::Rect(0, 0, 1, 1),
      ComputeResultRect(gfx::Rect(1, 1, 1, 2), kWeirdScaleFrom, kWeirdScaleTo));
  EXPECT_EQ(gfx::Rect(-1, -1, 1, 1),
            ComputeResultRect(gfx::Rect(-1, -1, 1, 2), kWeirdScaleFrom,
                              kWeirdScaleTo));
  EXPECT_EQ(
      gfx::Rect(2, 3, 2, 3),
      ComputeResultRect(gfx::Rect(3, 7, 3, 7), kWeirdScaleFrom, kWeirdScaleTo));
  EXPECT_EQ(gfx::Rect(-2, -3, 4, 6),
            ComputeResultRect(gfx::Rect(-3, -7, 6, 14), kWeirdScaleFrom,
                              kWeirdScaleTo));
  EXPECT_EQ(gfx::Rect((1 << 24) * 2 / 3, (1 << 25) * 3 / 7,
                      (1 << 15) * 2 / 3 + 1, (1 << 16) * 3 / 7 + 1),
            ComputeResultRect(gfx::Rect(1 << 24, 1 << 25, 1 << 15, 1 << 16),
                              kWeirdScaleFrom, kWeirdScaleTo));
  EXPECT_EQ(
      gfx::Rect(-(1 << 24) * 2 / 3 - 1, -(1 << 25) * 3 / 7 - 1,
                (1 << 15) * 2 / 3 + 1, (1 << 16) * 3 / 7 + 1),
      ComputeResultRect(gfx::Rect(-(1 << 24), -(1 << 25), 1 << 15, 1 << 16),
                        kWeirdScaleFrom, kWeirdScaleTo));
}

TEST(CopyOutputUtil, IdentifiesUnreasonableResultRects) {
  // When the scale ratio is too great for even a 1x1 rect to be scaled:
  EXPECT_EQ(gfx::Rect(),
            ComputeResultRect(gfx::Rect(1, 1, 1, 1), gfx::Vector2d(1, 1),
                              gfx::Vector2d(1 << 30, 1)));
  EXPECT_EQ(gfx::Rect(),
            ComputeResultRect(gfx::Rect(1, 1, 1, 1), gfx::Vector2d(1, 1),
                              gfx::Vector2d(1, 1 << 30)));
  EXPECT_EQ(gfx::Rect(),
            ComputeResultRect(gfx::Rect(1, 1, 1, 1), gfx::Vector2d(1, 1),
                              gfx::Vector2d(1 << 30, 1 << 30)));

  // When the rect and scale ratio are large, but produce unreasonable results:
  EXPECT_EQ(gfx::Rect(), ComputeResultRect(gfx::Rect(0, 0, 1 << 12, 1 << 13),
                                           gfx::Vector2d(1, 1),
                                           gfx::Vector2d(1 << 4, 1 << 3)));
  EXPECT_EQ(gfx::Rect(), ComputeResultRect(gfx::Rect(1 << 20, 1 << 21, 1, 1),
                                           gfx::Vector2d(1, 1),
                                           gfx::Vector2d(1 << 4, 1 << 3)));
  EXPECT_EQ(
      gfx::Rect(),
      ComputeResultRect(gfx::Rect(-(1 << 20), -(1 << 21), 1, 1),
                        gfx::Vector2d(1, 1), gfx::Vector2d(1 << 4, 1 << 3)));

  // Boundary condition: Right on the edge of "unreasonable."
  EXPECT_EQ(gfx::Rect((1 << 9) * ((1 << 15) - 1), 1, (1 << 15) - 1, 1),
            ComputeResultRect(gfx::Rect(1 << 9, 1, 1, 1), gfx::Vector2d(1, 1),
                              gfx::Vector2d((1 << 15) - 1, 1)));
  EXPECT_EQ(
      gfx::Rect(-(1 << 9) * ((1 << 15) - 1), 1, (1 << 15) - 1, 1),
      ComputeResultRect(gfx::Rect(-(1 << 9), 1, 1, 1), gfx::Vector2d(1, 1),
                        gfx::Vector2d((1 << 15) - 1, 1)));
  EXPECT_EQ(gfx::Rect(1, (1 << 9) * ((1 << 15) - 1), 1, (1 << 15) - 1),
            ComputeResultRect(gfx::Rect(1, 1 << 9, 1, 1), gfx::Vector2d(1, 1),
                              gfx::Vector2d(1, (1 << 15) - 1)));
  EXPECT_EQ(
      gfx::Rect(1, -(1 << 9) * ((1 << 15) - 1), 1, (1 << 15) - 1),
      ComputeResultRect(gfx::Rect(1, -(1 << 9), 1, 1), gfx::Vector2d(1, 1),
                        gfx::Vector2d(1, (1 << 15) - 1)));
}

}  // namespace

}  // namespace copy_output
}  // namespace viz
