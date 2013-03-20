// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/layer_test_common.h"

#include "cc/base/math_util.h"
#include "cc/base/region.h"
#include "cc/quads/draw_quad.h"
#include "cc/quads/render_pass.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/point_conversions.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/size_conversions.h"

namespace cc {

// Align with expected and actual output.
const char* LayerTestCommon::quad_string = "    Quad: ";

static bool CanRectFBeSafelyRoundedToRect(gfx::RectF r) {
  // Ensure that range of float values is not beyond integer range.
  if (!r.IsExpressibleAsRect())
    return false;

  // Ensure that the values are actually integers.
  if (gfx::ToFlooredPoint(r.origin()) == r.origin() &&
      gfx::ToFlooredSize(r.size()) == r.size())
    return true;

  return false;
}

void LayerTestCommon::VerifyQuadsExactlyCoverRect(const cc::QuadList& quads,
                                                  gfx::Rect rect) {
  cc::Region remaining = rect;

  for (size_t i = 0; i < quads.size(); ++i) {
    cc::DrawQuad* quad = quads[i];
    gfx::RectF quad_rectf =
        cc::MathUtil::MapClippedRect(quad->quadTransform(),
                                     gfx::RectF(quad->rect));

    // Before testing for exact coverage in the integer world, assert that
    // rounding will not round the rect incorrectly.
    ASSERT_TRUE(CanRectFBeSafelyRoundedToRect(quad_rectf));

    gfx::Rect quad_rect = gfx::ToEnclosingRect(quad_rectf);

    EXPECT_TRUE(rect.Contains(quad_rect)) << quad_string << i;
    EXPECT_TRUE(remaining.Contains(quad_rect)) << quad_string << i;
    remaining.Subtract(quad_rect);
  }

  EXPECT_TRUE(remaining.IsEmpty());
}

}  // namespace cc
