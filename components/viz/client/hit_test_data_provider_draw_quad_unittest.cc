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
#include "cc/test/fake_layer_tree_frame_sink_client.h"
#include "components/viz/client/local_surface_id_provider.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_gpu_memory_buffer_manager.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/viz/public/interfaces/compositing/compositor_frame_sink.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {

namespace {

CompositorFrame MakeCompositorFrameWithChildSurface(
    const SurfaceId& child_surface_id,
    const gfx::Rect& rect,
    const gfx::Rect& child_rect,
    const gfx::Transform& transform) {
  auto pass = RenderPass::Create();
  pass->SetNew(1, rect, rect, gfx::Transform());

  auto* shared_state = pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(transform, rect, rect, rect, false, false, 1,
                       SkBlendMode::kSrcOver, 0);

  auto* surface_quad = pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
  surface_quad->SetNew(pass->shared_quad_state_list.back(), child_rect,
                       child_rect, child_surface_id, base::nullopt,
                       SK_ColorWHITE, false);

  return CompositorFrameBuilder().AddRenderPass(std::move(pass)).Build();
}

}  // namespace

// Test to ensure that hit test data is created correctly from CompositorFrame
// and its RenderPassList. kHitTestAsk is only set for OOPIFs.
TEST(HitTestDataProviderDrawQuad, HitTestDataRenderer) {
  std::unique_ptr<HitTestDataProvider> hit_test_data_provider =
      std::make_unique<HitTestDataProviderDrawQuad>(
          true /* should_ask_for_child_region */);

  constexpr gfx::Rect kFrameRect(0, 0, 1024, 768);

  // Ensure that a CompositorFrame without a child surface sets kHitTestMine.
  CompositorFrame compositor_frame =
      CompositorFrameBuilder().AddRenderPass(kFrameRect, kFrameRect).Build();
  mojom::HitTestRegionListPtr hit_test_region_list =
      hit_test_data_provider->GetHitTestData(compositor_frame);

  EXPECT_EQ(mojom::kHitTestMouse | mojom::kHitTestTouch | mojom::kHitTestMine,
            hit_test_region_list->flags);
  EXPECT_EQ(kFrameRect, hit_test_region_list->bounds);
  EXPECT_FALSE(hit_test_region_list->regions.size());

  // Ensure that a CompositorFrame with a child surface only set kHitTestAsk
  // for its child surface.
  LocalSurfaceId child_local_surface_id(2, base::UnguessableToken::Create());
  FrameSinkId frame_sink_id(2, 0);
  SurfaceId child_surface_id(frame_sink_id, child_local_surface_id);
  gfx::Rect child_rect(200, 100);
  gfx::Transform transform;
  transform.Translate(-200, -100);
  compositor_frame = MakeCompositorFrameWithChildSurface(
      child_surface_id, kFrameRect, child_rect, transform);
  hit_test_region_list =
      hit_test_data_provider->GetHitTestData(compositor_frame);

  EXPECT_EQ(mojom::kHitTestMouse | mojom::kHitTestTouch | mojom::kHitTestMine,
            hit_test_region_list->flags);
  EXPECT_EQ(kFrameRect, hit_test_region_list->bounds);
  EXPECT_EQ(1u, hit_test_region_list->regions.size());
  EXPECT_EQ(child_surface_id.frame_sink_id(),
            hit_test_region_list->regions[0]->frame_sink_id);
  EXPECT_EQ(child_surface_id.local_surface_id(),
            hit_test_region_list->regions[0]->local_surface_id);
  EXPECT_EQ(mojom::kHitTestMouse | mojom::kHitTestTouch |
                mojom::kHitTestChildSurface | mojom::kHitTestAsk,
            hit_test_region_list->regions[0]->flags);
  EXPECT_EQ(child_rect, hit_test_region_list->regions[0]->rect);
  gfx::Transform transform_inverse;
  EXPECT_TRUE(transform.GetInverse(&transform_inverse));
  EXPECT_EQ(transform_inverse, hit_test_region_list->regions[0]->transform);
}

}  // namespace viz
