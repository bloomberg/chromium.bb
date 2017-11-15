// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/surface_layer_impl.h"

#include <stddef.h>

#include "cc/layers/append_quads_data.h"
#include "cc/test/layer_test_common.h"
#include "cc/trees/layer_tree_host_common.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::UnorderedElementsAre;

namespace cc {
namespace {

static constexpr viz::FrameSinkId kArbitraryFrameSinkId(1, 1);

TEST(SurfaceLayerImplTest, Occlusion) {
  gfx::Size layer_size(1000, 1000);
  gfx::Size viewport_size(1000, 1000);
  const viz::LocalSurfaceId kArbitraryLocalSurfaceId(
      9, base::UnguessableToken::Create());

  LayerTestCommon::LayerImplTest impl;

  SurfaceLayerImpl* surface_layer_impl =
      impl.AddChildToRoot<SurfaceLayerImpl>();
  surface_layer_impl->SetBounds(layer_size);
  surface_layer_impl->SetDrawsContent(true);
  viz::SurfaceId surface_id(kArbitraryFrameSinkId, kArbitraryLocalSurfaceId);
  surface_layer_impl->SetPrimarySurfaceInfo(
      viz::SurfaceInfo(surface_id, 1.f, layer_size));

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

// This test verifies that activation_dependencies and the fallback_surface_id
// are populated correctly if primary and fallback surfaces differ.
TEST(SurfaceLayerImplTest, SurfaceLayerImplWithTwoDifferentSurfaces) {
  LayerTestCommon::LayerImplTest impl;
  SurfaceLayerImpl* surface_layer_impl =
      impl.AddChildToRoot<SurfaceLayerImpl>();

  // Populate the primary viz::SurfaceInfo.
  const viz::LocalSurfaceId kArbitraryLocalSurfaceId1(
      9, base::UnguessableToken::Create());
  viz::SurfaceId surface_id1(kArbitraryFrameSinkId, kArbitraryLocalSurfaceId1);
  float surface_scale1 = 1.f;
  gfx::Size surface_size1(300, 300);
  viz::SurfaceInfo primary_surface_info(surface_id1, surface_scale1,
                                        surface_size1);

  // Populate the fallback viz::SurfaceId.
  const viz::LocalSurfaceId kArbitraryLocalSurfaceId2(
      7, base::UnguessableToken::Create());
  viz::SurfaceId surface_id2(kArbitraryFrameSinkId, kArbitraryLocalSurfaceId2);

  gfx::Size layer_size(400, 100);

  // Populate the SurfaceLayerImpl ensuring that the primary and fallback
  // SurfaceInfos are different.
  surface_layer_impl->SetBounds(layer_size);
  surface_layer_impl->SetDrawsContent(true);
  surface_layer_impl->SetPrimarySurfaceInfo(primary_surface_info);
  surface_layer_impl->SetFallbackSurfaceId(surface_id2);
  surface_layer_impl->SetDefaultBackgroundColor(SK_ColorBLUE);

  gfx::Size viewport_size(1000, 1000);
  impl.CalcDrawProps(viewport_size);

  std::unique_ptr<viz::RenderPass> render_pass = viz::RenderPass::Create();
  {
    AppendQuadsData data;
    surface_layer_impl->AppendQuads(render_pass.get(), &data);
    // The the primary viz::SurfaceInfo will be added to
    // activation_dependencies.
    EXPECT_THAT(data.activation_dependencies,
                UnorderedElementsAre(surface_id1));
  }

  // Update the fallback to an invalid viz::SurfaceInfo. The
  // |activation_dependencies| should still contain the primary
  // viz::SurfaceInfo.
  {
    AppendQuadsData data;
    surface_layer_impl->SetFallbackSurfaceId(viz::SurfaceId());
    surface_layer_impl->AppendQuads(render_pass.get(), &data);
    // The primary viz::SurfaceInfo should not be added to
    // activation_dependencies.
    EXPECT_THAT(data.activation_dependencies,
                UnorderedElementsAre(surface_id1));
  }

  // Update the fallback viz::SurfaceInfo and re-emit DrawQuads.
  {
    AppendQuadsData data;
    surface_layer_impl->SetFallbackSurfaceId(surface_id2);
    surface_layer_impl->AppendQuads(render_pass.get(), &data);
    // The the primary viz::SurfaceInfo will be added to
    // activation_dependencies.
    EXPECT_THAT(data.activation_dependencies,
                UnorderedElementsAre(surface_id1));
  }

  ASSERT_EQ(3u, render_pass->quad_list.size());
  const viz::SurfaceDrawQuad* surface_draw_quad1 =
      viz::SurfaceDrawQuad::MaterialCast(render_pass->quad_list.ElementAt(0));
  ASSERT_TRUE(surface_draw_quad1);
  const viz::SurfaceDrawQuad* surface_draw_quad2 =
      viz::SurfaceDrawQuad::MaterialCast(render_pass->quad_list.ElementAt(1));
  ASSERT_TRUE(surface_draw_quad2);
  const viz::SurfaceDrawQuad* surface_draw_quad3 =
      viz::SurfaceDrawQuad::MaterialCast(render_pass->quad_list.ElementAt(2));
  ASSERT_TRUE(surface_draw_quad3);

  EXPECT_EQ(surface_id1, surface_draw_quad1->primary_surface_id);
  EXPECT_EQ(SK_ColorBLUE, surface_draw_quad1->default_background_color);
  EXPECT_EQ(surface_id2, surface_draw_quad1->fallback_surface_id);

  EXPECT_EQ(surface_id1, surface_draw_quad2->primary_surface_id);
  EXPECT_EQ(SK_ColorBLUE, surface_draw_quad2->default_background_color);
  EXPECT_EQ(base::nullopt, surface_draw_quad2->fallback_surface_id);

  EXPECT_EQ(surface_id1, surface_draw_quad3->primary_surface_id);
  EXPECT_EQ(SK_ColorBLUE, surface_draw_quad3->default_background_color);
  EXPECT_EQ(surface_id2, surface_draw_quad3->fallback_surface_id);
}

// This test verifies that one viz::SurfaceDrawQuad is emitted if a
// SurfaceLayerImpl holds the same surface ID for both the primary
// and fallback viz::SurfaceInfo.
TEST(SurfaceLayerImplTest, SurfaceLayerImplWithMatchingPrimaryAndFallback) {
  LayerTestCommon::LayerImplTest impl;
  SurfaceLayerImpl* surface_layer_impl =
      impl.AddChildToRoot<SurfaceLayerImpl>();

  // Populate the primary viz::SurfaceInfo.
  const viz::LocalSurfaceId kArbitraryLocalSurfaceId1(
      9, base::UnguessableToken::Create());
  viz::SurfaceId surface_id1(kArbitraryFrameSinkId, kArbitraryLocalSurfaceId1);
  float surface_scale1 = 1.f;
  gfx::Size surface_size1(300, 300);
  viz::SurfaceInfo primary_surface_info(surface_id1, surface_scale1,
                                        surface_size1);

  gfx::Size layer_size(400, 100);

  // Populate the SurfaceLayerImpl ensuring that the primary and fallback
  // SurfaceInfos are the same.
  surface_layer_impl->SetBounds(layer_size);
  surface_layer_impl->SetDrawsContent(true);
  surface_layer_impl->SetPrimarySurfaceInfo(primary_surface_info);
  surface_layer_impl->SetFallbackSurfaceId(primary_surface_info.id());
  surface_layer_impl->SetDefaultBackgroundColor(SK_ColorBLUE);

  gfx::Size viewport_size(1000, 1000);
  impl.CalcDrawProps(viewport_size);

  std::unique_ptr<viz::RenderPass> render_pass = viz::RenderPass::Create();
  AppendQuadsData data;
  surface_layer_impl->AppendQuads(render_pass.get(), &data);
  // As the primary and fallback SurfaceInfos match, there is no reason to
  // add the primary surface ID to |activation_dependencies| because it is not
  // an unresolved dependency. The fallback surface will already be added as a
  // reference in referenced_surfaces.
  EXPECT_THAT(data.activation_dependencies, testing::IsEmpty());

  ASSERT_EQ(1u, render_pass->quad_list.size());
  const viz::SurfaceDrawQuad* surface_draw_quad1 =
      viz::SurfaceDrawQuad::MaterialCast(render_pass->quad_list.ElementAt(0));
  ASSERT_TRUE(surface_draw_quad1);

  EXPECT_EQ(surface_id1, surface_draw_quad1->primary_surface_id);
  EXPECT_EQ(surface_id1, surface_draw_quad1->fallback_surface_id);
  EXPECT_EQ(SK_ColorBLUE, surface_draw_quad1->default_background_color);
}

}  // namespace
}  // namespace cc
