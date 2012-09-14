// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCMathUtil.h"
#include "FloatQuad.h"
#include <gtest/gtest.h>
#include <public/WebTransformationMatrix.h>

using namespace cc;
using WebKit::WebTransformationMatrix;

namespace {

TEST(FloatQuadTest, IsRectilinearTest)
{
    const int numRectilinear = 8;
    WebTransformationMatrix rectilinearTrans[numRectilinear];
    rectilinearTrans[1].rotate(90);
    rectilinearTrans[2].rotate(180);
    rectilinearTrans[3].rotate(270);
    rectilinearTrans[4].skewX(0.00000000001);
    rectilinearTrans[5].skewY(0.00000000001);
    rectilinearTrans[6].scale(0.00001);
    rectilinearTrans[6].rotate(180);
    rectilinearTrans[7].scale(100000);
    rectilinearTrans[7].rotate(180);

    for (int i = 0; i < numRectilinear; ++i) {
        bool clipped = false;
        FloatQuad quad = CCMathUtil::mapQuad(rectilinearTrans[i], FloatRect(0.01010101f, 0.01010101f, 100.01010101f, 100.01010101f), clipped);
        ASSERT_TRUE(!clipped);
        EXPECT_TRUE(quad.isRectilinear());
    }

    const int numNonRectilinear = 10;
    WebTransformationMatrix nonRectilinearTrans[numNonRectilinear];
    nonRectilinearTrans[0].rotate(359.999);
    nonRectilinearTrans[1].rotate(0.0000001);
    nonRectilinearTrans[2].rotate(89.999999);
    nonRectilinearTrans[3].rotate(90.0000001);
    nonRectilinearTrans[4].rotate(179.999999);
    nonRectilinearTrans[5].rotate(180.0000001);
    nonRectilinearTrans[6].rotate(269.999999);
    nonRectilinearTrans[7].rotate(270.0000001);
    nonRectilinearTrans[8].skewX(0.00001);
    nonRectilinearTrans[9].skewY(0.00001);

    for (int i = 0; i < numNonRectilinear; ++i) {
        bool clipped = false;
        FloatQuad quad = CCMathUtil::mapQuad(nonRectilinearTrans[i], FloatRect(0.01010101f, 0.01010101f, 100.01010101f, 100.01010101f), clipped);
        ASSERT_TRUE(!clipped);
        EXPECT_FALSE(quad.isRectilinear());
    }
}

} // empty namespace
