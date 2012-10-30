// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/test/layer_test_common.h"

#include "cc/draw_quad.h"
#include "cc/math_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace LayerTestCommon {

// Align with expected and actual output
const char* quadString = "    Quad: ";

bool floatRectCanBeSafelyRoundedToIntRect(const cc::FloatRect& r)
{
    // Ensure that range of float values is not beyond integer range.
    if (!r.isExpressibleAsIntRect())
        return false;

    // Ensure that the values are actually integers.
    if (floorf(r.x()) == r.x()
        && floorf(r.y()) == r.y()
        && floorf(r.width()) == r.width()
        && floorf(r.height()) == r.height())
        return true;

    return false;
}

void verifyQuadsExactlyCoverRect(const cc::QuadList& quads,
                                 const cc::IntRect& rect) {
    cc::Region remaining(rect);

    for (size_t i = 0; i < quads.size(); ++i) {
        cc::DrawQuad* quad = quads[i];
        cc::FloatRect floatQuadRect = cc::MathUtil::mapClippedRect(quad->sharedQuadState()->quadTransform, cc::FloatRect(quad->quadRect()));

        // Before testing for exact coverage in the integer world, assert that rounding
        // will not round the rect incorrectly.
        ASSERT_TRUE(floatRectCanBeSafelyRoundedToIntRect(floatQuadRect));

        cc::IntRect quadRect = enclosingIntRect(floatQuadRect);

        EXPECT_TRUE(rect.contains(quadRect)) << quadString << i;
        EXPECT_TRUE(remaining.contains(quadRect)) << quadString << i;
        remaining.subtract(cc::Region(quadRect));
    }

    EXPECT_TRUE(remaining.isEmpty());
}

}  // namespace LayerTestCommon
