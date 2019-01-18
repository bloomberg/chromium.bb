// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/client/hit_test_data_provider_draw_quad.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "components/viz/client/local_surface_id_provider.h"
#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "components/viz/common/quads/render_pass_draw_quad.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_gpu_memory_buffer_manager.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {

namespace {

SurfaceId CreateChildSurfaceId(uint32_t id) {
  LocalSurfaceId child_local_surface_id(id, base::UnguessableToken::Create());
  FrameSinkId frame_sink_id(id, 0);
  SurfaceId child_surface_id(frame_sink_id, child_local_surface_id);
  return child_surface_id;
}

std::unique_ptr<RenderPass> CreateRenderPassWithChildSurface(
    RenderPassId render_pass_id,
    const SurfaceId& child_surface_id,
    const gfx::Rect& rect,
    const gfx::Rect& child_rect,
    const gfx::Transform& render_pass_transform,
    const gfx::Transform& shared_state_transform,
    const base::Optional<SurfaceId>& fallback_child_surface_id =
        base::nullopt) {
  auto pass = RenderPass::Create();
  pass->SetNew(render_pass_id, rect, rect, render_pass_transform);

  auto* shared_state = pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(shared_state_transform, rect, rect, rect, false, false,
                       1, SkBlendMode::kSrcOver, 0);

  auto* surface_quad = pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
  surface_quad->SetNew(
      pass->shared_quad_state_list.back(), child_rect, child_rect,
      SurfaceRange(fallback_child_surface_id, child_surface_id), SK_ColorWHITE,
      /*stretch_content_to_fill_bounds=*/false, /*ignores_input_event=*/false);

  return pass;
}

}  // namespace

// Test to ensure that hit test data is created correctly from CompositorFrame
// and its RenderPassList. kHitTestAsk is only set for OOPIFs.
TEST(HitTestDataProviderDrawQuad, HitTestDataRenderer) {
  std::unique_ptr<HitTestDataProvider> hit_test_data_provider =
      std::make_unique<HitTestDataProviderDrawQuad>(
          true /* should_ask_for_child_region */,
          true /* root_accepts_events */);

  constexpr gfx::Rect kFrameRect(0, 0, 1024, 768);

  // Ensure that a CompositorFrame without a child surface sets kHitTestMine.
  CompositorFrame compositor_frame =
      CompositorFrameBuilder().AddRenderPass(kFrameRect, kFrameRect).Build();
  base::Optional<HitTestRegionList> hit_test_region_list =
      hit_test_data_provider->GetHitTestData(compositor_frame);

  EXPECT_EQ(HitTestRegionFlags::kHitTestMouse |
                HitTestRegionFlags::kHitTestTouch |
                HitTestRegionFlags::kHitTestMine,
            hit_test_region_list->flags);
  EXPECT_EQ(kFrameRect, hit_test_region_list->bounds);
  EXPECT_FALSE(hit_test_region_list->regions.size());

  // Ensure that a CompositorFrame with a child surface only set kHitTestAsk
  // for its child surface.
  SurfaceId child_surface_id = CreateChildSurfaceId(2);
  gfx::Rect child_rect(200, 100);
  gfx::Transform render_pass_transform;
  render_pass_transform.Translate(-50, -100);
  render_pass_transform.Skew(2, 3);
  gfx::Transform shared_state_transform;
  shared_state_transform.Translate(-200, -100);
  auto pass = CreateRenderPassWithChildSurface(
      1, child_surface_id, kFrameRect, child_rect, render_pass_transform,
      shared_state_transform);
  compositor_frame =
      CompositorFrameBuilder().AddRenderPass(std::move(pass)).Build();
  hit_test_region_list =
      hit_test_data_provider->GetHitTestData(compositor_frame);

  EXPECT_EQ(HitTestRegionFlags::kHitTestMouse |
                HitTestRegionFlags::kHitTestTouch |
                HitTestRegionFlags::kHitTestMine,
            hit_test_region_list->flags);
  EXPECT_EQ(kFrameRect, hit_test_region_list->bounds);
  EXPECT_EQ(1u, hit_test_region_list->regions.size());
  EXPECT_EQ(child_surface_id.frame_sink_id(),
            hit_test_region_list->regions[0].frame_sink_id);
  EXPECT_EQ(HitTestRegionFlags::kHitTestMouse |
                HitTestRegionFlags::kHitTestTouch |
                HitTestRegionFlags::kHitTestChildSurface |
                HitTestRegionFlags::kHitTestAsk,
            hit_test_region_list->regions[0].flags);
  EXPECT_EQ(child_rect, hit_test_region_list->regions[0].rect);
  gfx::Transform render_pass_transform_inverse;
  EXPECT_TRUE(render_pass_transform.GetInverse(&render_pass_transform_inverse));
  gfx::Transform shared_state_transform_inverse;
  EXPECT_TRUE(
      shared_state_transform.GetInverse(&shared_state_transform_inverse));
  EXPECT_EQ(shared_state_transform_inverse * render_pass_transform_inverse,
            hit_test_region_list->regions[0].transform);
}

// Test to ensure that we skip regions with non-invertible transforms and with
// updated FrameSinkId between fallback and primary when preparing for hit-test
// data.
TEST(HitTestDataProviderDrawQuad, HitTestDataSkipQuads) {
  std::unique_ptr<HitTestDataProvider> hit_test_data_provider =
      std::make_unique<HitTestDataProviderDrawQuad>(
          true /* should_ask_for_child_region */,
          true /* root_accepts_events */);

  constexpr gfx::Rect kFrameRect(0, 0, 1024, 768);
  gfx::Rect child_rect(200, 100);

  // A degenerate matrix of all zeros is not invertible.
  gfx::Transform not_invertible_transform;
  not_invertible_transform.matrix().set(0, 0, 0.f);
  not_invertible_transform.matrix().set(1, 1, 0.f);
  not_invertible_transform.matrix().set(2, 2, 0.f);
  not_invertible_transform.matrix().set(3, 3, 0.f);

  gfx::Transform invertible_transform;
  invertible_transform.Translate(-200, -100);

  RenderPassList pass_list;

  // A render pass that has non-invertible transform.
  SurfaceId child_surface_id1 = CreateChildSurfaceId(2);
  auto pass1 = CreateRenderPassWithChildSurface(
      1, child_surface_id1, kFrameRect, child_rect, not_invertible_transform,
      invertible_transform);
  pass_list.push_back(std::move(pass1));

  // A render pass with a draw quad that has non-invertible transform.
  SurfaceId child_surface_id2 = CreateChildSurfaceId(3);
  auto pass2 = CreateRenderPassWithChildSurface(
      2, child_surface_id2, kFrameRect, child_rect, invertible_transform,
      not_invertible_transform);
  pass_list.push_back(std::move(pass2));

  // A render pass and its draw quad both have invertible transforms.
  SurfaceId child_surface_id3 = CreateChildSurfaceId(4);
  auto pass3 = CreateRenderPassWithChildSurface(
      3, child_surface_id3, kFrameRect, child_rect, invertible_transform,
      invertible_transform);
  pass_list.push_back(std::move(pass3));

  // The draw quad's FrameSinkId changed between fallback and primary.
  SurfaceId child_surface_id4 = CreateChildSurfaceId(5);
  SurfaceId fallback_child_surface_id4 = CreateChildSurfaceId(6);
  auto pass4 = CreateRenderPassWithChildSurface(
      4, child_surface_id4, kFrameRect, child_rect, invertible_transform,
      invertible_transform, fallback_child_surface_id4);
  pass_list.push_back(std::move(pass4));

  auto pass5_root = RenderPass::Create();
  pass5_root->output_rect = kFrameRect;
  pass5_root->id = 5;
  auto* shared_quad_state5_root = pass5_root->CreateAndAppendSharedQuadState();
  gfx::Rect rect5_root(kFrameRect);
  shared_quad_state5_root->SetAll(
      gfx::Transform(), /*quad_layer_rect=*/rect5_root,
      /*visible_quad_layer_rect=*/rect5_root, /*clip_rect=*/rect5_root,
      /*is_clipped=*/false, /*are_contents_opaque=*/false,
      /*opacity=*/0.5f, SkBlendMode::kSrcOver, /*sorting_context_id=*/0);
  auto* quad5_root_1 =
      pass5_root->quad_list.AllocateAndConstruct<RenderPassDrawQuad>();
  quad5_root_1->SetNew(shared_quad_state5_root, /*rect=*/rect5_root,
                       /*visible_rect=*/rect5_root, /*render_pass_id=*/1,
                       /*mask_resource_id=*/0, gfx::RectF(), gfx::Size(),
                       gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false,
                       1.0f);
  auto* quad5_root_2 =
      pass5_root->quad_list.AllocateAndConstruct<RenderPassDrawQuad>();
  quad5_root_2->SetNew(shared_quad_state5_root, /*rect=*/rect5_root,
                       /*visible_rect=*/rect5_root, /*render_pass_id=*/2,
                       /*mask_resource_id=*/0, gfx::RectF(), gfx::Size(),
                       gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false,
                       1.0f);
  auto* quad5_root_3 =
      pass5_root->quad_list.AllocateAndConstruct<RenderPassDrawQuad>();
  quad5_root_3->SetNew(shared_quad_state5_root, /*rect=*/rect5_root,
                       /*visible_rect=*/rect5_root, /*render_pass_id=*/3,
                       /*mask_resource_id=*/0, gfx::RectF(), gfx::Size(),
                       gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false,
                       1.0f);
  auto* quad5_root_4 =
      pass5_root->quad_list.AllocateAndConstruct<RenderPassDrawQuad>();
  quad5_root_4->SetNew(shared_quad_state5_root, /*rect=*/rect5_root,
                       /*visible_rect=*/rect5_root, /*render_pass_id=*/4,
                       /*mask_resource_id=*/0, gfx::RectF(), gfx::Size(),
                       gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false,
                       1.0f);
  pass_list.push_back(std::move(pass5_root));

  auto compositor_frame =
      CompositorFrameBuilder().SetRenderPassList(std::move(pass_list)).Build();
  base::Optional<HitTestRegionList> hit_test_region_list =
      hit_test_data_provider->GetHitTestData(compositor_frame);

  // Only pass3 should have a hit-test region that corresponds to
  // child_surface_id3.
  EXPECT_EQ(1u, hit_test_region_list->regions.size());
  EXPECT_EQ(child_surface_id3.frame_sink_id(),
            hit_test_region_list->regions[0].frame_sink_id);
}

// Test to ensure that browser shouldn't set kHitTestAsk flag for the renderers
// it embeds.
TEST(HitTestDataProviderDrawQuad, HitTestDataBrowser) {
  std::unique_ptr<HitTestDataProvider> hit_test_data_provider =
      std::make_unique<HitTestDataProviderDrawQuad>(
          false /* should_ask_for_child_region */,
          true /* root_accepts_events */);

  constexpr gfx::Rect frame_rect(1024, 768);
  SurfaceId child_surface_id = CreateChildSurfaceId(2);
  constexpr gfx::Rect child_rect(200, 100);
  gfx::Transform render_to_browser_transform;
  render_to_browser_transform.Translate(-200, -100);
  auto pass = CreateRenderPassWithChildSurface(1, child_surface_id, frame_rect,
                                               child_rect, gfx::Transform(),
                                               render_to_browser_transform);
  CompositorFrame compositor_frame =
      CompositorFrameBuilder().AddRenderPass(std::move(pass)).Build();
  base::Optional<HitTestRegionList> hit_test_region_list =
      hit_test_data_provider->GetHitTestData(compositor_frame);

  // Browser should be able to receive both mouse and touch events. It embeds
  // one renderer, which should also have mouse and touch flags; the renderer
  // should have child surface flag because it is being embeded, but not ask
  // flag because we shouldn't do asyn targeting for the entire renderer.
  EXPECT_EQ(HitTestRegionFlags::kHitTestMouse |
                HitTestRegionFlags::kHitTestTouch |
                HitTestRegionFlags::kHitTestMine,
            hit_test_region_list->flags);
  EXPECT_EQ(frame_rect, hit_test_region_list->bounds);
  EXPECT_EQ(1u, hit_test_region_list->regions.size());
  EXPECT_EQ(child_surface_id.frame_sink_id(),
            hit_test_region_list->regions[0].frame_sink_id);
  EXPECT_EQ(HitTestRegionFlags::kHitTestMouse |
                HitTestRegionFlags::kHitTestTouch |
                HitTestRegionFlags::kHitTestChildSurface,
            hit_test_region_list->regions[0].flags);
  EXPECT_EQ(child_rect, hit_test_region_list->regions[0].rect);
  gfx::Transform browser_to_render_transform;
  EXPECT_TRUE(
      render_to_browser_transform.GetInverse(&browser_to_render_transform));
  EXPECT_EQ(browser_to_render_transform,
            hit_test_region_list->regions[0].transform);
}

// Test to ensure that we should set kHitTestIgnore flag for transparent
// windows.
TEST(HitTestDataProviderDrawQuad, HitTestDataTransparent) {
  std::unique_ptr<HitTestDataProvider> hit_test_data_provider =
      std::make_unique<HitTestDataProviderDrawQuad>(
          true /* should_ask_for_child_region */,
          false /* root_accepts_events */);

  constexpr gfx::Rect frame_rect(1024, 768);
  SurfaceId child_surface_id = CreateChildSurfaceId(2);
  constexpr gfx::Rect child_rect(200, 100);
  gfx::Transform child_to_parent_transform;
  child_to_parent_transform.Translate(-200, -100);
  auto pass = CreateRenderPassWithChildSurface(1, child_surface_id, frame_rect,
                                               child_rect, gfx::Transform(),
                                               child_to_parent_transform);
  CompositorFrame compositor_frame =
      CompositorFrameBuilder().AddRenderPass(std::move(pass)).Build();
  base::Optional<HitTestRegionList> hit_test_region_list =
      hit_test_data_provider->GetHitTestData(compositor_frame);

  EXPECT_EQ(HitTestRegionFlags::kHitTestMouse |
                HitTestRegionFlags::kHitTestTouch |
                HitTestRegionFlags::kHitTestIgnore,
            hit_test_region_list->flags);
  EXPECT_EQ(frame_rect, hit_test_region_list->bounds);
  EXPECT_EQ(1u, hit_test_region_list->regions.size());
  EXPECT_EQ(child_surface_id.frame_sink_id(),
            hit_test_region_list->regions[0].frame_sink_id);
  EXPECT_EQ(HitTestRegionFlags::kHitTestMouse |
                HitTestRegionFlags::kHitTestTouch |
                HitTestRegionFlags::kHitTestChildSurface |
                HitTestRegionFlags::kHitTestAsk,
            hit_test_region_list->regions[0].flags);
  EXPECT_EQ(child_rect, hit_test_region_list->regions[0].rect);
  gfx::Transform parent_to_child_transform;
  EXPECT_TRUE(child_to_parent_transform.GetInverse(&parent_to_child_transform));
  EXPECT_EQ(parent_to_child_transform,
            hit_test_region_list->regions[0].transform);
}

}  // namespace viz
