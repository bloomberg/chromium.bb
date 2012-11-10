// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layer_quad.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/quad_f.h"

namespace cc {
namespace {

TEST(LayerQuadTest, QuadFConversion)
{
    gfx::PointF p1(-0.5, -0.5);
    gfx::PointF p2( 0.5, -0.5);
    gfx::PointF p3( 0.5,  0.5);
    gfx::PointF p4(-0.5,  0.5);

    gfx::QuadF quadCW(p1, p2, p3, p4);
    LayerQuad layerQuadCW(quadCW);
    EXPECT_TRUE(layerQuadCW.ToQuadF() == quadCW);

    gfx::QuadF quadCCW(p1, p4, p3, p2);
    LayerQuad layerQuadCCW(quadCCW);
    EXPECT_TRUE(layerQuadCCW.ToQuadF() == quadCCW);
}

TEST(LayerQuadTest, Inflate)
{
    gfx::PointF p1(-0.5, -0.5);
    gfx::PointF p2( 0.5, -0.5);
    gfx::PointF p3( 0.5,  0.5);
    gfx::PointF p4(-0.5,  0.5);

    gfx::QuadF quad(p1, p2, p3, p4);
    LayerQuad layerQuad(quad);
    quad.Scale(2, 2);
    layerQuad.inflate(0.5);
    EXPECT_TRUE(layerQuad.ToQuadF() == quad);
}

}  // namespace
}  // namespace cc
