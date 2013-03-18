// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/math_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/quad_f.h"
#include "ui/gfx/rect_f.h"
#include "ui/gfx/transform.h"

namespace cc {
namespace {

// TODO(danakj) Move this test to ui/gfx/ when we don't need MathUtil::MapQuad.
TEST(FloatQuadTest, IsRectilinearTest) {
  const int kNumRectilinear = 8;
  gfx::Transform rectilinear_trans[kNumRectilinear];
  rectilinear_trans[1].Rotate(90.0);
  rectilinear_trans[2].Rotate(180.0);
  rectilinear_trans[3].Rotate(270.0);
  rectilinear_trans[4].SkewX(0.00000000001);
  rectilinear_trans[5].SkewY(0.00000000001);
  rectilinear_trans[6].Scale(0.00001, 0.00001);
  rectilinear_trans[6].Rotate(180.0);
  rectilinear_trans[7].Scale(100000, 100000);
  rectilinear_trans[7].Rotate(180.0);

  for (int i = 0; i < kNumRectilinear; ++i) {
    bool clipped = false;
    gfx::QuadF quad = MathUtil::MapQuad(
        rectilinear_trans[i],
        gfx::QuadF(
            gfx::RectF(0.01010101f, 0.01010101f, 100.01010101f, 100.01010101f)),
        &clipped);
    ASSERT_TRUE(!clipped);
    EXPECT_TRUE(quad.IsRectilinear());
  }

  const int kNumNonRectilinear = 10;
  gfx::Transform non_rectilinear_trans[kNumNonRectilinear];
  non_rectilinear_trans[0].Rotate(359.999);
  non_rectilinear_trans[1].Rotate(0.0000001);
  non_rectilinear_trans[2].Rotate(89.999999);
  non_rectilinear_trans[3].Rotate(90.0000001);
  non_rectilinear_trans[4].Rotate(179.999999);
  non_rectilinear_trans[5].Rotate(180.0000001);
  non_rectilinear_trans[6].Rotate(269.999999);
  non_rectilinear_trans[7].Rotate(270.0000001);
  non_rectilinear_trans[8].SkewX(0.00001);
  non_rectilinear_trans[9].SkewY(0.00001);

  for (int i = 0; i < kNumNonRectilinear; ++i) {
    bool clipped = false;
    gfx::QuadF quad = MathUtil::MapQuad(
        non_rectilinear_trans[i],
        gfx::QuadF(
            gfx::RectF(0.01010101f, 0.01010101f, 100.01010101f, 100.01010101f)),
        &clipped);
    ASSERT_TRUE(!clipped);
    EXPECT_FALSE(quad.IsRectilinear());
  }
}

}  // namespace
}  // namespace cc
