// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/layer_quad.h"

#include "testing/gtest/include/gtest/gtest.h"

using namespace cc;

namespace {

TEST(LayerQuadTest, FloatQuadConversion)
{
    FloatPoint p1(-0.5, -0.5);
    FloatPoint p2( 0.5, -0.5);
    FloatPoint p3( 0.5,  0.5);
    FloatPoint p4(-0.5,  0.5);

    FloatQuad quadCW(p1, p2, p3, p4);
    LayerQuad layerQuadCW(quadCW);
    EXPECT_TRUE(layerQuadCW.floatQuad() == quadCW);

    FloatQuad quadCCW(p1, p4, p3, p2);
    LayerQuad layerQuadCCW(quadCCW);
    EXPECT_TRUE(layerQuadCCW.floatQuad() == quadCCW);
}

TEST(LayerQuadTest, Inflate)
{
    FloatPoint p1(-0.5, -0.5);
    FloatPoint p2( 0.5, -0.5);
    FloatPoint p3( 0.5,  0.5);
    FloatPoint p4(-0.5,  0.5);

    FloatQuad quad(p1, p2, p3, p4);
    LayerQuad layerQuad(quad);
    quad.scale(2, 2);
    layerQuad.inflate(0.5);
    EXPECT_TRUE(layerQuad.floatQuad() == quad);
}

} // namespace
