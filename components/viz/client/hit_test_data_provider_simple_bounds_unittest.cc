// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/client/hit_test_data_provider_simple_bounds.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "cc/test/fake_layer_tree_frame_sink_client.h"
#include "cc/test/test_context_provider.h"
#include "components/viz/client/local_surface_id_provider.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "components/viz/test/test_gpu_memory_buffer_manager.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/viz/public/interfaces/compositing/compositor_frame_sink.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {

namespace {

CompositorFrame MakeCompositorFrameWithChildSurface(gfx::Rect rect) {
  auto pass = RenderPass::Create();
  pass->SetNew(1, rect, rect, gfx::Transform());

  auto* shared_state = pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(gfx::Transform(), rect, rect, rect, false, false, 1,
                       SkBlendMode::kSrcOver, 0);

  gfx::Rect child_rect(200, 100);

  LocalSurfaceId child_local_surface_id(2, base::UnguessableToken::Create());
  FrameSinkId frame_sink_id(2, 0);
  SurfaceId child_surface_id(frame_sink_id, child_local_surface_id);

  auto* surface_quad = pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
  surface_quad->SetNew(pass->shared_quad_state_list.back(), child_rect,
                       child_rect, child_surface_id, base::nullopt,
                       SK_ColorWHITE, false);

  return CompositorFrameBuilder().AddRenderPass(std::move(pass)).Build();
}

}  // namespace

// Test to ensure that when hit test data is created from a CompositorFrame
// that the size matches the bounds and that kHitTestMine is set when there
// is not embedded surface and kHitTestAsk is set when there is an embedded
// surface.
TEST(HitTestDataProviderSimpleBounds, HitTestDataFromCompositorFrame) {
  std::unique_ptr<HitTestDataProvider> hit_test_data_provider =
      std::make_unique<HitTestDataProviderSimpleBounds>();

  constexpr gfx::Rect kFrameRect(0, 0, 1024, 768);

  // Ensure that a CompositorFrame without a child surface sets kHitTestMine.
  CompositorFrame compositor_frame =
      CompositorFrameBuilder().AddRenderPass(kFrameRect, kFrameRect).Build();
  mojom::HitTestRegionListPtr hit_test_region_list =
      hit_test_data_provider->GetHitTestData(compositor_frame);

  EXPECT_EQ(mojom::kHitTestMouse | mojom::kHitTestTouch | mojom::kHitTestMine,
            hit_test_region_list->flags);
  EXPECT_EQ(kFrameRect, hit_test_region_list->bounds);

  // Ensure that a CompositorFrame with a child surface sets kHitTestAsk.
  compositor_frame = MakeCompositorFrameWithChildSurface(kFrameRect);
  hit_test_region_list =
      hit_test_data_provider->GetHitTestData(compositor_frame);

  EXPECT_EQ(mojom::kHitTestMouse | mojom::kHitTestTouch | mojom::kHitTestAsk,
            hit_test_region_list->flags);
  EXPECT_EQ(kFrameRect, hit_test_region_list->bounds);
}

}  // namespace viz
