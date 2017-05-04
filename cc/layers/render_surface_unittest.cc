// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/append_quads_data.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/render_surface_impl.h"
#include "cc/quads/shared_quad_state.h"
#include "cc/test/fake_compositor_frame_sink.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/mock_occlusion_tracker.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/single_thread_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/transform.h"

namespace cc {
namespace {

#define EXECUTE_AND_VERIFY_SURFACE_CHANGED(code_to_test) \
  render_surface->ResetPropertyChangedFlags();           \
  code_to_test;                                          \
  EXPECT_TRUE(render_surface->SurfacePropertyChanged())

#define EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(code_to_test) \
  render_surface->ResetPropertyChangedFlags();                  \
  code_to_test;                                                 \
  EXPECT_FALSE(render_surface->SurfacePropertyChanged())

TEST(RenderSurfaceTest, VerifySurfaceChangesAreTrackedProperly) {
  //
  // This test checks that SurfacePropertyChanged() has the correct behavior.
  //

  FakeImplTaskRunnerProvider task_runner_provider;
  TestTaskGraphRunner task_graph_runner;
  std::unique_ptr<CompositorFrameSink> compositor_frame_sink =
      FakeCompositorFrameSink::Create3d();
  FakeLayerTreeHostImpl host_impl(&task_runner_provider, &task_graph_runner);
  std::unique_ptr<LayerImpl> owning_layer =
      LayerImpl::Create(host_impl.active_tree(), 1);
  owning_layer->test_properties()->force_render_surface = true;
  gfx::Rect test_rect(3, 4, 5, 6);
  host_impl.active_tree()->ResetAllChangeTracking();
  host_impl.active_tree()->SetRootLayerForTesting(std::move(owning_layer));
  host_impl.SetVisible(true);
  host_impl.InitializeRenderer(compositor_frame_sink.get());
  host_impl.active_tree()->BuildLayerListAndPropertyTreesForTesting();
  host_impl.active_tree()->UpdateDrawProperties(false /* update_lcd_text */);

  RenderSurfaceImpl* render_surface =
      GetRenderSurface(host_impl.active_tree()->root_layer_for_testing());
  ASSERT_TRUE(render_surface);

  // Currently, the content_rect, clip_rect, and
  // owning_layer->layerPropertyChanged() are the only sources of change.
  EXECUTE_AND_VERIFY_SURFACE_CHANGED(render_surface->SetClipRect(test_rect));
  EXECUTE_AND_VERIFY_SURFACE_CHANGED(
      render_surface->SetContentRectForTesting(test_rect));

  host_impl.active_tree()->SetOpacityMutated(
      host_impl.active_tree()->root_layer_for_testing()->element_id(), 0.5f);
  EXPECT_TRUE(render_surface->SurfacePropertyChanged());
  host_impl.active_tree()->ResetAllChangeTracking();

  // Setting the surface properties to the same values again should not be
  // considered "change".
  EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(
      render_surface->SetClipRect(test_rect));
  EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(
      render_surface->SetContentRectForTesting(test_rect));

  std::unique_ptr<LayerImpl> dummy_mask =
      LayerImpl::Create(host_impl.active_tree(), 2);
  gfx::Transform dummy_matrix;
  dummy_matrix.Translate(1.0, 2.0);

  // The rest of the surface properties are either internal and should not cause
  // change, or they are already accounted for by the
  // owninglayer->layerPropertyChanged().
  EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(
      render_surface->SetDrawOpacity(0.5f));
  EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(
      render_surface->SetDrawTransform(dummy_matrix));
}

TEST(RenderSurfaceTest, SanityCheckSurfaceCreatesCorrectSharedQuadState) {
  FakeImplTaskRunnerProvider task_runner_provider;
  TestTaskGraphRunner task_graph_runner;
  std::unique_ptr<CompositorFrameSink> compositor_frame_sink =
      FakeCompositorFrameSink::Create3d();
  FakeLayerTreeHostImpl host_impl(&task_runner_provider, &task_graph_runner);
  std::unique_ptr<LayerImpl> root_layer =
      LayerImpl::Create(host_impl.active_tree(), 1);

  int owning_layer_id = 2;
  std::unique_ptr<LayerImpl> owning_layer =
      LayerImpl::Create(host_impl.active_tree(), owning_layer_id);
  owning_layer->test_properties()->force_render_surface = true;

  SkBlendMode blend_mode = SkBlendMode::kSoftLight;
  owning_layer->test_properties()->blend_mode = blend_mode;

  root_layer->test_properties()->AddChild(std::move(owning_layer));
  host_impl.active_tree()->SetRootLayerForTesting(std::move(root_layer));
  host_impl.SetVisible(true);
  host_impl.InitializeRenderer(compositor_frame_sink.get());
  host_impl.active_tree()->BuildLayerListAndPropertyTreesForTesting();
  host_impl.active_tree()->UpdateDrawProperties(false /* update_lcd_text */);

  ASSERT_TRUE(
      GetRenderSurface(host_impl.active_tree()->LayerById(owning_layer_id)));
  RenderSurfaceImpl* render_surface =
      GetRenderSurface(host_impl.active_tree()->LayerById(owning_layer_id));

  gfx::Rect content_rect(0, 0, 50, 50);
  gfx::Rect clip_rect(5, 5, 40, 40);
  gfx::Transform origin;
  origin.Translate(30, 40);

  render_surface->SetContentRectForTesting(content_rect);
  render_surface->SetClipRect(clip_rect);
  render_surface->SetDrawOpacity(1.f);
  render_surface->SetDrawTransform(origin);

  std::unique_ptr<RenderPass> render_pass = RenderPass::Create();
  AppendQuadsData append_quads_data;

  render_surface->AppendQuads(render_pass.get(), &append_quads_data);

  ASSERT_EQ(1u, render_pass->shared_quad_state_list.size());
  SharedQuadState* shared_quad_state =
      render_pass->shared_quad_state_list.front();

  EXPECT_EQ(
      30.0,
      shared_quad_state->quad_to_target_transform.matrix().getDouble(0, 3));
  EXPECT_EQ(
      40.0,
      shared_quad_state->quad_to_target_transform.matrix().getDouble(1, 3));
  EXPECT_EQ(content_rect,
            gfx::Rect(shared_quad_state->visible_quad_layer_rect));
  EXPECT_EQ(1.f, shared_quad_state->opacity);
  EXPECT_EQ(blend_mode, shared_quad_state->blend_mode);
}


TEST(RenderSurfaceTest, SanityCheckSurfaceCreatesCorrectRenderPass) {
  FakeImplTaskRunnerProvider task_runner_provider;
  TestTaskGraphRunner task_graph_runner;
  std::unique_ptr<CompositorFrameSink> compositor_frame_sink =
      FakeCompositorFrameSink::Create3d();
  FakeLayerTreeHostImpl host_impl(&task_runner_provider, &task_graph_runner);
  std::unique_ptr<LayerImpl> root_layer =
      LayerImpl::Create(host_impl.active_tree(), 1);

  int owning_layer_id = 2;
  std::unique_ptr<LayerImpl> owning_layer =
      LayerImpl::Create(host_impl.active_tree(), owning_layer_id);
  owning_layer->test_properties()->force_render_surface = true;

  root_layer->test_properties()->AddChild(std::move(owning_layer));
  host_impl.active_tree()->SetRootLayerForTesting(std::move(root_layer));
  host_impl.SetVisible(true);
  host_impl.InitializeRenderer(compositor_frame_sink.get());
  host_impl.active_tree()->BuildLayerListAndPropertyTreesForTesting();
  host_impl.active_tree()->UpdateDrawProperties(false /* update_lcd_text */);

  ASSERT_TRUE(
      GetRenderSurface(host_impl.active_tree()->LayerById(owning_layer_id)));
  RenderSurfaceImpl* render_surface =
      GetRenderSurface(host_impl.active_tree()->LayerById(owning_layer_id));

  gfx::Rect content_rect(0, 0, 50, 50);
  gfx::Transform origin;
  origin.Translate(30.0, 40.0);

  render_surface->SetScreenSpaceTransform(origin);
  render_surface->SetContentRectForTesting(content_rect);

  auto pass = render_surface->CreateRenderPass();

  EXPECT_EQ(2, pass->id);
  EXPECT_EQ(content_rect, pass->output_rect);
  EXPECT_EQ(origin, pass->transform_to_root_target);
}

}  // namespace
}  // namespace cc
