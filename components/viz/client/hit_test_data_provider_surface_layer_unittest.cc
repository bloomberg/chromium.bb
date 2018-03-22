// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/client/hit_test_data_provider_surface_layer.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "cc/layers/surface_layer_impl.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_frame_sink_client.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/layer_tree_impl.h"
#include "components/viz/client/local_surface_id_provider.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "components/viz/test/test_gpu_memory_buffer_manager.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/viz/public/interfaces/compositing/compositor_frame_sink.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {

// Test to ensure that hit test data is created correctly from CompositorFrame
// and the underlying layer tree.
TEST(HitTestDataProviderSurfaceLayerTest, HitTestDataRenderer) {
  cc::FakeImplTaskRunnerProvider task_runner_provider;
  cc::TestTaskGraphRunner task_graph_runner;
  cc::FakeLayerTreeHostImpl host_impl(&task_runner_provider,
                                      &task_graph_runner);

#if DCHECK_IS_ON()
  task_runner_provider.SetCurrentThreadIsImplThread(true);
#endif

  std::unique_ptr<HitTestDataProvider> hit_test_data_provider =
      std::make_unique<HitTestDataProviderSurfaceLayer>();

  // Setup surface layers in LayerTreeHostImpl.

  host_impl.CreatePendingTree();
  host_impl.ActivateSyncTree();

  // The structure of the layer tree:
  // +-Root (1024x768)
  // +---intermediate_layer (200, 300), 200x200
  // +-----surface_child1 (50, 50), 100x100, Rotate(45)
  // +---surface_child2 (450, 300), 100x100
  // +---overlapping_layer (500, 350), 200x200
  std::unique_ptr<cc::LayerImpl> intermediate_layer =
      cc::LayerImpl::Create(host_impl.active_tree(), 2);
  std::unique_ptr<cc::SurfaceLayerImpl> surface_child1 =
      cc::SurfaceLayerImpl::Create(host_impl.active_tree(), 3);
  std::unique_ptr<cc::SurfaceLayerImpl> surface_child2 =
      cc::SurfaceLayerImpl::Create(host_impl.active_tree(), 4);
  std::unique_ptr<cc::LayerImpl> overlapping_layer =
      cc::LayerImpl::Create(host_impl.active_tree(), 5);

  host_impl.SetViewportSize(gfx::Size(1024, 768));

  intermediate_layer->SetPosition(gfx::PointF(200, 300));
  intermediate_layer->SetBounds(gfx::Size(200, 200));

  surface_child1->SetPosition(gfx::PointF(50, 50));
  surface_child1->SetBounds(gfx::Size(100, 100));
  gfx::Transform rotate;
  rotate.Rotate(45);
  surface_child1->test_properties()->transform = rotate;
  surface_child1->SetDrawsContent(true);
  surface_child1->SetHitTestable(true);

  surface_child2->SetPosition(gfx::PointF(450, 300));
  surface_child2->SetBounds(gfx::Size(100, 100));
  surface_child2->SetDrawsContent(true);
  surface_child2->SetHitTestable(true);

  overlapping_layer->SetPosition(gfx::PointF(500, 350));
  overlapping_layer->SetBounds(gfx::Size(200, 200));
  overlapping_layer->SetDrawsContent(true);

  LocalSurfaceId child_local_surface_id(2, base::UnguessableToken::Create());
  FrameSinkId frame_sink_id(2, 0);
  SurfaceId child_surface_id(frame_sink_id, child_local_surface_id);
  surface_child1->SetPrimarySurfaceId(child_surface_id, base::nullopt);
  surface_child2->SetPrimarySurfaceId(child_surface_id, base::nullopt);

  std::unique_ptr<cc::LayerImpl> root =
      cc::LayerImpl::Create(host_impl.active_tree(), 1);
  host_impl.active_tree()->SetRootLayerForTesting(std::move(root));
  intermediate_layer->test_properties()->AddChild(std::move(surface_child1));
  host_impl.active_tree()
      ->root_layer_for_testing()
      ->test_properties()
      ->AddChild(std::move(intermediate_layer));
  host_impl.active_tree()
      ->root_layer_for_testing()
      ->test_properties()
      ->AddChild(std::move(surface_child2));
  host_impl.active_tree()
      ->root_layer_for_testing()
      ->test_properties()
      ->AddChild(std::move(overlapping_layer));

  host_impl.active_tree()->BuildPropertyTreesForTesting();

  hit_test_data_provider->UpdateLayerTreeHostImpl(&host_impl);
  constexpr gfx::Rect kFrameRect(0, 0, 1024, 768);

  CompositorFrame compositor_frame =
      CompositorFrameBuilder().AddRenderPass(kFrameRect, kFrameRect).Build();
  mojom::HitTestRegionListPtr hit_test_region_list =
      hit_test_data_provider->GetHitTestData(compositor_frame);

  // Since surface_child2 draws in front of surface_child1, it should also be in
  // the front of the hit test region list.
  EXPECT_EQ(mojom::kHitTestMouse | mojom::kHitTestTouch | mojom::kHitTestMine,
            hit_test_region_list->flags);
  EXPECT_EQ(kFrameRect, hit_test_region_list->bounds);
  EXPECT_EQ(2u, hit_test_region_list->regions.size());

  EXPECT_EQ(child_surface_id.frame_sink_id(),
            hit_test_region_list->regions[1]->frame_sink_id);
  EXPECT_EQ(
      mojom::kHitTestMouse | mojom::kHitTestTouch | mojom::kHitTestChildSurface,
      hit_test_region_list->regions[1]->flags);
  gfx::Transform child1_transform;
  child1_transform.Rotate(-45);
  child1_transform.Translate(-250, -350);
  EXPECT_TRUE(child1_transform.ApproximatelyEqual(
      hit_test_region_list->regions[1]->transform));
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(),
            hit_test_region_list->regions[1]->rect.ToString());

  EXPECT_EQ(child_surface_id.frame_sink_id(),
            hit_test_region_list->regions[0]->frame_sink_id);
  EXPECT_EQ(mojom::kHitTestMouse | mojom::kHitTestTouch |
                mojom::kHitTestChildSurface | mojom::kHitTestAsk,
            hit_test_region_list->regions[0]->flags);
  gfx::Transform child2_transform;
  child2_transform.Translate(-450, -300);
  EXPECT_TRUE(child2_transform.ApproximatelyEqual(
      hit_test_region_list->regions[0]->transform));
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(),
            hit_test_region_list->regions[0]->rect.ToString());
}

}  // namespace viz
