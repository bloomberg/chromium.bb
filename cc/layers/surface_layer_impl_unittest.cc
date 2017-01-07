// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/surface_layer_impl.h"

#include <stddef.h>

#include "cc/layers/append_quads_data.h"
#include "cc/test/layer_test_common.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

static constexpr FrameSinkId kArbitraryFrameSinkId(1, 1);

TEST(SurfaceLayerImplTest, Occlusion) {
  gfx::Size layer_size(1000, 1000);
  gfx::Size viewport_size(1000, 1000);
  const LocalFrameId kArbitraryLocalFrameId(9,
                                            base::UnguessableToken::Create());

  LayerTestCommon::LayerImplTest impl;

  SurfaceLayerImpl* surface_layer_impl =
      impl.AddChildToRoot<SurfaceLayerImpl>();
  surface_layer_impl->SetBounds(layer_size);
  surface_layer_impl->SetDrawsContent(true);
  SurfaceId surface_id(kArbitraryFrameSinkId, kArbitraryLocalFrameId);
  surface_layer_impl->SetSurfaceInfo(SurfaceInfo(surface_id, 1.f, layer_size));

  impl.CalcDrawProps(viewport_size);

  {
    SCOPED_TRACE("No occlusion");
    gfx::Rect occluded;
    impl.AppendQuadsWithOcclusion(surface_layer_impl, occluded);

    LayerTestCommon::VerifyQuadsExactlyCoverRect(impl.quad_list(),
                                                 gfx::Rect(layer_size));
    EXPECT_EQ(1u, impl.quad_list().size());
  }

  {
    SCOPED_TRACE("Full occlusion");
    gfx::Rect occluded(surface_layer_impl->visible_layer_rect());
    impl.AppendQuadsWithOcclusion(surface_layer_impl, occluded);

    LayerTestCommon::VerifyQuadsExactlyCoverRect(impl.quad_list(), gfx::Rect());
    EXPECT_EQ(impl.quad_list().size(), 0u);
  }

  {
    SCOPED_TRACE("Partial occlusion");
    gfx::Rect occluded(200, 0, 800, 1000);
    impl.AppendQuadsWithOcclusion(surface_layer_impl, occluded);

    size_t partially_occluded_count = 0;
    LayerTestCommon::VerifyQuadsAreOccluded(
        impl.quad_list(), occluded, &partially_occluded_count);
    // The layer outputs one quad, which is partially occluded.
    EXPECT_EQ(1u, impl.quad_list().size());
    EXPECT_EQ(1u, partially_occluded_count);
  }
}

TEST(SurfaceLayerImplTest, SurfaceStretchedToLayerBounds) {
  LayerTestCommon::LayerImplTest impl;
  SurfaceLayerImpl* surface_layer_impl =
      impl.AddChildToRoot<SurfaceLayerImpl>();
  const LocalFrameId kArbitraryLocalFrameId(9,
                                            base::UnguessableToken::Create());

  // Given condition: layer and surface have different size and different
  // aspect ratios.
  gfx::Size layer_size(400, 100);
  gfx::Size surface_size(300, 300);
  gfx::Size viewport_size(1000, 1000);
  float surface_scale = 1.f;
  gfx::Transform target_space_transform(
      surface_layer_impl->draw_properties().target_space_transform);

  // The following code is mimicking the PushPropertiesTo from pending to
  // active tree.
  surface_layer_impl->SetBounds(layer_size);
  surface_layer_impl->SetDrawsContent(true);
  SurfaceId surface_id(kArbitraryFrameSinkId, kArbitraryLocalFrameId);
  surface_layer_impl->SetSurfaceInfo(
      SurfaceInfo(surface_id, surface_scale, surface_size));
  surface_layer_impl->SetStretchContentToFillBounds(true);

  impl.CalcDrawProps(viewport_size);

  std::unique_ptr<RenderPass> render_pass = RenderPass::Create();
  AppendQuadsData data;
  surface_layer_impl->AppendQuads(render_pass.get(), &data);

  const QuadList& quads = render_pass->quad_list;
  ASSERT_EQ(1u, quads.size());
  const SharedQuadState* shared_quad_state = quads.front()->shared_quad_state;

  // We expect that the transform for the quad stretches the quad to cover the
  // entire bounds of the layer.
  gfx::Transform expected_transform(target_space_transform);
  float scale_x = static_cast<float>(surface_size.width()) / layer_size.width();
  float scale_y =
      static_cast<float>(surface_size.height()) / layer_size.height();
  expected_transform.Scale(SK_MScalar1 / scale_x, SK_MScalar1 / scale_y);
  EXPECT_EQ(expected_transform, shared_quad_state->quad_to_target_transform);

  // Obtain quad rect in target space by applying SQS->quad_to_target_transform
  // to quad_rect
  gfx::RectF quad_rect(quads.front()->rect);
  gfx::RectF transformed_quad_rect =
      MathUtil::MapClippedRect(expected_transform, quad_rect);

  // Obtain layer rect in target space by applying target_space_transform on
  // layer rect.
  gfx::RectF layer_rect(layer_size.width(), layer_size.height());
  gfx::RectF transformed_layer_rect =
      MathUtil::MapClippedRect(target_space_transform, layer_rect);

  // Check if quad rect in target space matches layer rect in target space
  EXPECT_EQ(transformed_quad_rect, transformed_layer_rect);
}

}  // namespace
}  // namespace cc
