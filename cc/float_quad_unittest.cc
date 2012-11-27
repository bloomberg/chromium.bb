// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/math_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/rect_f.h"
#include "ui/gfx/quad_f.h"
#include "ui/gfx/transform.h"

namespace cc {
namespace {

// TODO(danakj) Move this test to ui/gfx/ when we don't need MathUtil::mapQuad.
TEST(FloatQuadTest, IsRectilinearTest)
{
    const int numRectilinear = 8;
    gfx::Transform rectilinearTrans[numRectilinear];
    rectilinearTrans[1].Rotate(90);
    rectilinearTrans[2].Rotate(180);
    rectilinearTrans[3].Rotate(270);
    rectilinearTrans[4].SkewX(0.00000000001);
    rectilinearTrans[5].SkewY(0.00000000001);
    rectilinearTrans[6].Scale(0.00001, 0.00001);
    rectilinearTrans[6].Rotate(180);
    rectilinearTrans[7].Scale(100000, 100000);
    rectilinearTrans[7].Rotate(180);

    for (int i = 0; i < numRectilinear; ++i) {
        bool clipped = false;
        gfx::QuadF quad = MathUtil::mapQuad(rectilinearTrans[i], gfx::QuadF(gfx::RectF(0.01010101f, 0.01010101f, 100.01010101f, 100.01010101f)), clipped);
        ASSERT_TRUE(!clipped);
        EXPECT_TRUE(quad.IsRectilinear());
    }

    const int numNonRectilinear = 10;
    gfx::Transform nonRectilinearTrans[numNonRectilinear];
    nonRectilinearTrans[0].Rotate(359.999);
    nonRectilinearTrans[1].Rotate(0.0000001);
    nonRectilinearTrans[2].Rotate(89.999999);
    nonRectilinearTrans[3].Rotate(90.0000001);
    nonRectilinearTrans[4].Rotate(179.999999);
    nonRectilinearTrans[5].Rotate(180.0000001);
    nonRectilinearTrans[6].Rotate(269.999999);
    nonRectilinearTrans[7].Rotate(270.0000001);
    nonRectilinearTrans[8].SkewX(0.00001);
    nonRectilinearTrans[9].SkewY(0.00001);

    for (int i = 0; i < numNonRectilinear; ++i) {
        bool clipped = false;
        gfx::QuadF quad = MathUtil::mapQuad(nonRectilinearTrans[i], gfx::QuadF(gfx::RectF(0.01010101f, 0.01010101f, 100.01010101f, 100.01010101f)), clipped);
        ASSERT_TRUE(!clipped);
        EXPECT_FALSE(quad.IsRectilinear());
    }
}

}  // namespace
}  // namespace cc
