// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/layer_test_common.h"

#include "cc/draw_quad.h"
#include "cc/math_util.h"
#include "cc/region.h"
#include "cc/render_pass.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/point_conversions.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/size_conversions.h"

namespace cc {
namespace LayerTestCommon {

// Align with expected and actual output
const char* quadString = "    Quad: ";

bool canRectFBeSafelyRoundedToRect(const gfx::RectF& r)
{
    // Ensure that range of float values is not beyond integer range.
    if (!r.IsExpressibleAsRect())
        return false;

    // Ensure that the values are actually integers.
    if (gfx::ToFlooredPoint(r.origin()) == r.origin() && gfx::ToFlooredSize(r.size()) == r.size())
        return true;

    return false;
}

void verifyQuadsExactlyCoverRect(const cc::QuadList& quads,
                                 const gfx::Rect& rect) {
    cc::Region remaining = rect;

    for (size_t i = 0; i < quads.size(); ++i) {
        cc::DrawQuad* quad = quads[i];
        gfx::RectF quadRectF = cc::MathUtil::mapClippedRect(quad->quadTransform(), gfx::RectF(quad->rect));

        // Before testing for exact coverage in the integer world, assert that rounding
        // will not round the rect incorrectly.
        ASSERT_TRUE(canRectFBeSafelyRoundedToRect(quadRectF));

        gfx::Rect quadRect = gfx::ToEnclosingRect(quadRectF);

        EXPECT_TRUE(rect.Contains(quadRect)) << quadString << i;
        EXPECT_TRUE(remaining.Contains(quadRect)) << quadString << i;
        remaining.Subtract(quadRect);
    }

    EXPECT_TRUE(remaining.IsEmpty());
}

}  // namespace LayerTestCommon
}  // namespace cc
