// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/layer_test_common.h"

#include "cc/base/math_util.h"
#include "cc/base/region.h"
#include "cc/layers/append_quads_data.h"
#include "cc/quads/draw_quad.h"
#include "cc/quads/render_pass.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/mock_occlusion_tracker.h"
#include "cc/trees/layer_tree_host_common.h"
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

  size_t i = 0;
  for (QuadList::ConstIterator iter = quads.begin(); iter != quads.end();
       ++iter) {
    const DrawQuad* quad = &*iter;
    gfx::RectF quad_rectf =
        MathUtil::MapClippedRect(quad->quadTransform(), gfx::RectF(quad->rect));

    // Before testing for exact coverage in the integer world, assert that
    // rounding will not round the rect incorrectly.
    ASSERT_TRUE(CanRectFBeSafelyRoundedToRect(quad_rectf));

    gfx::Rect quad_rect = gfx::ToEnclosingRect(quad_rectf);

    EXPECT_TRUE(rect.Contains(quad_rect)) << quad_string << i
                                          << " rect: " << rect.ToString()
                                          << " quad: " << quad_rect.ToString();
    EXPECT_TRUE(remaining.Contains(quad_rect))
        << quad_string << i << " remaining: " << remaining.ToString()
        << " quad: " << quad_rect.ToString();
    remaining.Subtract(quad_rect);

    ++i;
  }

  EXPECT_TRUE(remaining.IsEmpty());
}

// static
void LayerTestCommon::VerifyQuadsAreOccluded(const QuadList& quads,
                                             const gfx::Rect& occluded,
                                             size_t* partially_occluded_count) {
  // No quad should exist if it's fully occluded.
  for (QuadList::ConstIterator iter = quads.begin(); iter != quads.end();
       ++iter) {
    gfx::Rect target_visible_rect = MathUtil::MapEnclosingClippedRect(
        iter->quadTransform(), iter->visible_rect);
    EXPECT_FALSE(occluded.Contains(target_visible_rect));
  }

  // Quads that are fully occluded on one axis only should be shrunken.
  for (QuadList::ConstIterator iter = quads.begin(); iter != quads.end();
       ++iter) {
    const DrawQuad* quad = &*iter;
    DCHECK(quad->quadTransform().IsIdentityOrIntegerTranslation());
    gfx::Rect target_rect =
        MathUtil::MapEnclosingClippedRect(quad->quadTransform(), quad->rect);
    gfx::Rect target_visible_rect = MathUtil::MapEnclosingClippedRect(
        quad->quadTransform(), quad->visible_rect);

    bool fully_occluded_horizontal = target_rect.x() >= occluded.x() &&
                                     target_rect.right() <= occluded.right();
    bool fully_occluded_vertical = target_rect.y() >= occluded.y() &&
                                   target_rect.bottom() <= occluded.bottom();
    bool should_be_occluded =
        target_rect.Intersects(occluded) &&
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
    : client_(FakeLayerTreeHostClient::DIRECT_3D),
      host_(FakeLayerTreeHost::Create(&client_)),
      root_layer_impl_(LayerImpl::Create(host_->host_impl()->active_tree(), 1)),
      render_pass_(RenderPass::Create()) {
  scoped_ptr<FakeOutputSurface> output_surface = FakeOutputSurface::Create3d();
  host_->host_impl()->InitializeRenderer(
      output_surface.PassAs<OutputSurface>());
}

LayerTestCommon::LayerImplTest::~LayerImplTest() {}

void LayerTestCommon::LayerImplTest::CalcDrawProps(
    const gfx::Size& viewport_size) {
  LayerImplList layer_list;
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root_layer_impl_.get(), viewport_size, &layer_list);
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);
}

void LayerTestCommon::LayerImplTest::AppendQuadsWithOcclusion(
    LayerImpl* layer_impl,
    const gfx::Rect& occluded) {
  AppendQuadsData data;

  render_pass_->quad_list.clear();
  render_pass_->shared_quad_state_list.clear();
  occlusion_tracker_.set_occluded_target_rect(occluded);
  layer_impl->WillDraw(DRAW_MODE_HARDWARE, resource_provider());
  layer_impl->AppendQuads(render_pass_.get(), occlusion_tracker_, &data);
  layer_impl->DidDraw(resource_provider());
}

void LayerTestCommon::LayerImplTest::AppendQuadsForPassWithOcclusion(
    LayerImpl* layer_impl,
    const RenderPassId& id,
    const gfx::Rect& occluded) {
  AppendQuadsData data(id);

  render_pass_->quad_list.clear();
  render_pass_->shared_quad_state_list.clear();
  occlusion_tracker_.set_occluded_target_rect(occluded);
  layer_impl->WillDraw(DRAW_MODE_HARDWARE, resource_provider());
  layer_impl->AppendQuads(render_pass_.get(), occlusion_tracker_, &data);
  layer_impl->DidDraw(resource_provider());
}

void LayerTestCommon::LayerImplTest::AppendSurfaceQuadsWithOcclusion(
    RenderSurfaceImpl* surface_impl,
    const gfx::Rect& occluded) {
  AppendQuadsData data;

  render_pass_->quad_list.clear();
  render_pass_->shared_quad_state_list.clear();
  occlusion_tracker_.set_occluded_target_rect_for_contributing_surface(
      occluded);
  bool for_replica = false;
  RenderPassId id(1, 1);
  surface_impl->AppendQuads(
      render_pass_.get(), occlusion_tracker_, &data, for_replica, id);
}

}  // namespace cc
