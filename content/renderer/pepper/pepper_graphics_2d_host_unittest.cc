// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_graphics_2d_host.h"

#include "base/basictypes.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"

namespace content {

class PepperGraphics2DHostTest : public testing::Test {
 public:
  static bool ConvertToLogicalPixels(float scale,
                                     gfx::Rect* op_rect,
                                     gfx::Point* delta) {
    return PepperGraphics2DHost::ConvertToLogicalPixels(scale, op_rect, delta);
  }
};

TEST_F(PepperGraphics2DHostTest, ConvertToLogicalPixels) {
  static const struct {
    int x1;
    int y1;
    int w1;
    int h1;
    int x2;
    int y2;
    int w2;
    int h2;
    int dx1;
    int dy1;
    int dx2;
    int dy2;
    float scale;
    bool result;
  } tests[] = {
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1.0, true },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2.0, true },
    { 0, 0, 4, 4, 0, 0, 2, 2, 0, 0, 0, 0, 0.5, true },
    { 1, 1, 4, 4, 0, 0, 3, 3, 0, 0, 0, 0, 0.5, false },
    { 53, 75, 100, 100, 53, 75, 100, 100, 0, 0, 0, 0, 1.0, true },
    { 53, 75, 100, 100, 106, 150, 200, 200, 0, 0, 0, 0, 2.0, true },
    { 53, 75, 100, 100, 26, 37, 51, 51, 0, 0, 0, 0, 0.5, false },
    { 53, 74, 100, 100, 26, 37, 51, 50, 0, 0, 0, 0, 0.5, false },
    { -1, -1, 100, 100, -1, -1, 51, 51, 0, 0, 0, 0, 0.5, false },
    { -2, -2, 100, 100, -1, -1, 50, 50, 4, -4, 2, -2, 0.5, true },
    { -101, -100, 50, 50, -51, -50, 26, 25, 0, 0, 0, 0, 0.5, false },
    { 10, 10, 20, 20, 5, 5, 10, 10, 0, 0, 0, 0, 0.5, true },
      // Cannot scroll due to partial coverage on sides
    { 11, 10, 20, 20, 5, 5, 11, 10, 0, 0, 0, 0, 0.5, false },
      // Can scroll since backing store is actually smaller/scaling up
    { 11, 20, 100, 100, 22, 40, 200, 200, 7, 3, 14, 6, 2.0, true },
      // Can scroll due to delta and bounds being aligned
    { 10, 10, 20, 20, 5, 5, 10, 10, 6, 4, 3, 2, 0.5, true },
      // Cannot scroll due to dx
    { 10, 10, 20, 20, 5, 5, 10, 10, 5, 4, 2, 2, 0.5, false },
      // Cannot scroll due to dy
    { 10, 10, 20, 20, 5, 5, 10, 10, 6, 3, 3, 1, 0.5, false },
      // Cannot scroll due to top
    { 10, 11, 20, 20, 5, 5, 10, 11, 6, 4, 3, 2, 0.5, false },
      // Cannot scroll due to left
    { 7, 10, 20, 20, 3, 5, 11, 10, 6, 4, 3, 2, 0.5, false },
      // Cannot scroll due to width
    { 10, 10, 21, 20, 5, 5, 11, 10, 6, 4, 3, 2, 0.5, false },
      // Cannot scroll due to height
    { 10, 10, 20, 51, 5, 5, 10, 26, 6, 4, 3, 2, 0.5, false },
      // Check negative scroll deltas
    { 10, 10, 20, 20, 5, 5, 10, 10, -6, -4, -3, -2, 0.5, true },
    { 10, 10, 20, 20, 5, 5, 10, 10, -6, -3, -3, -1, 0.5, false },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    gfx::Rect r1(tests[i].x1, tests[i].y1, tests[i].w1, tests[i].h1);
    gfx::Rect r2(tests[i].x2, tests[i].y2, tests[i].w2, tests[i].h2);
    gfx::Rect orig = r1;
    gfx::Point delta(tests[i].dx1, tests[i].dy1);
    bool res = ConvertToLogicalPixels(tests[i].scale, &r1, &delta);
    EXPECT_EQ(r2.ToString(), r1.ToString());
    EXPECT_EQ(res, tests[i].result);
    if (res) {
      EXPECT_EQ(delta, gfx::Point(tests[i].dx2, tests[i].dy2));
    }
    // Reverse the scale and ensure all the original pixels are still inside
    // the result.
    ConvertToLogicalPixels(1.0f / tests[i].scale, &r1, NULL);
    EXPECT_TRUE(r1.Contains(orig));
  }
}

}  // namespace content
