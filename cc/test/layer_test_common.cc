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

static bool CanRectFBeSafelyRoundedToRect(const gfx::RectF& r) {
  // Ensure that range of float values is not beyond integer range.
  if (!r.IsExpressibleAsRect())
    return false;

  // Ensure that the values are actually integers.
  if (gfx::ToFlooredPoint(r.origin()) == r.origin() &&
      gfx::ToFlooredSize(r.size()) == r.size())
    return true;

  return false;
}

void LayerTestCommon::VerifyQuadsExactlyCoverRect(const QuadList& quads,
                                                  const gfx::Rect& rect) {
  Region remaining = rect;

  for (size_t i = 0; i < quads.size(); ++i) {
    DrawQuad* quad = quads[i];
    gfx::RectF quad_rectf =
        MathUtil::MapClippedRect(quad->quadTransform(), gfx::RectF(quad->rect));

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

// static
void LayerTestCommon::VerifyQuadsCoverRectWithOcclusion(
    const QuadList& quads,
    const gfx::Rect& rect,
    const gfx::Rect& occluded,
    size_t* partially_occluded_count) {
  // No quad should exist if it's fully occluded.
  for (size_t i = 0; i < quads.size(); ++i) {
    EXPECT_FALSE(occluded.Contains(quads[i]->visible_rect));
  }

  // Quads that are fully occluded on one axis only should be shrunken.
  for (size_t i = 0; i < quads.size(); ++i) {
    DrawQuad* quad = quads[i];
    bool fully_occluded_horizontal = quad->rect.x() >= occluded.x() &&
                                     quad->rect.right() <= occluded.right();
    bool fully_occluded_vertical = quad->rect.y() >= occluded.y() &&
                                   quad->rect.bottom() <= occluded.bottom();
    bool should_be_occluded =
        quad->rect.Intersects(occluded) &&
        (fully_occluded_vertical || fully_occluded_horizontal);
    if (!should_be_occluded) {
      EXPECT_EQ(quad->rect.ToString(), quad->visible_rect.ToString());
    } else {
      EXPECT_NE(quad->rect.ToString(), quad->visible_rect.ToString());
      EXPECT_TRUE(quad->rect.Contains(quad->visible_rect));
      ++(*partially_occluded_count);
    }
  }
}

LayerTestCommon::LayerImplTest::LayerImplTest()
    : host_(FakeLayerTreeHost::Create()),
      root_layer_impl_(
          LayerImpl::Create(host_->host_impl()->active_tree(), 1)) {}

LayerTestCommon::LayerImplTest::~LayerImplTest() {}

}  // namespace cc
