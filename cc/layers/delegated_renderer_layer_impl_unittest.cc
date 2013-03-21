// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/delegated_renderer_layer_impl.h"

#include "cc/base/scoped_ptr_vector.h"
#include "cc/layers/append_quads_data.h"
#include "cc/layers/quad_sink.h"
#include "cc/layers/solid_color_layer_impl.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/test/fake_delegated_renderer_layer_impl.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_layer_tree_host_impl_client.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_proxy.h"
#include "cc/test/fake_rendering_stats_instrumentation.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/mock_quad_culler.h"
#include "cc/test/render_pass_test_common.h"
#include "cc/test/render_pass_test_utils.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/single_thread_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/transform.h"

namespace cc {
namespace {

class DelegatedRendererLayerImplTest : public testing::Test {
 public:
  DelegatedRendererLayerImplTest()
      : proxy_(scoped_ptr<Thread>(NULL))
      , always_impl_thread_and_main_thread_blocked_(&proxy_) {
    LayerTreeSettings settings;
    settings.minimum_occlusion_tracking_size = gfx::Size();

    host_impl_ = LayerTreeHostImpl::Create(settings,
                                           &client_,
                                           &proxy_,
                                           &stats_instrumentation_);
    host_impl_->InitializeRenderer(CreateFakeOutputSurface());
    host_impl_->SetViewportSize(gfx::Size(10, 10), gfx::Size(10, 10));
  }

 protected:
  FakeProxy proxy_;
  FakeLayerTreeHostImplClient client_;
  DebugScopedSetImplThreadAndMainThreadBlocked
      always_impl_thread_and_main_thread_blocked_;
  FakeRenderingStatsInstrumentation stats_instrumentation_;
  scoped_ptr<LayerTreeHostImpl> host_impl_;
};

class DelegatedRendererLayerImplTestSimple
    : public DelegatedRendererLayerImplTest {
 public:
  DelegatedRendererLayerImplTestSimple()
      : DelegatedRendererLayerImplTest() {
    scoped_ptr<LayerImpl> root_layer = SolidColorLayerImpl::Create(
        host_impl_->active_tree(), 1).PassAs<LayerImpl>();
    scoped_ptr<LayerImpl> layer_before = SolidColorLayerImpl::Create(
        host_impl_->active_tree(), 2).PassAs<LayerImpl>();
    scoped_ptr<LayerImpl> layer_after = SolidColorLayerImpl::Create(
        host_impl_->active_tree(), 3).PassAs<LayerImpl>();
    scoped_ptr<FakeDelegatedRendererLayerImpl> delegated_renderer_layer =
        FakeDelegatedRendererLayerImpl::Create(host_impl_->active_tree(), 4);

    host_impl_->SetViewportSize(gfx::Size(100, 100), gfx::Size(100, 100));
    root_layer->SetBounds(gfx::Size(100, 100));

    layer_before->SetPosition(gfx::Point(20, 20));
    layer_before->SetBounds(gfx::Size(14, 14));
    layer_before->SetContentBounds(gfx::Size(14, 14));
    layer_before->SetDrawsContent(true);
    layer_before->SetForceRenderSurface(true);

    layer_after->SetPosition(gfx::Point(5, 5));
    layer_after->SetBounds(gfx::Size(15, 15));
    layer_after->SetContentBounds(gfx::Size(15, 15));
    layer_after->SetDrawsContent(true);
    layer_after->SetForceRenderSurface(true);

    delegated_renderer_layer->SetPosition(gfx::Point(3, 3));
    delegated_renderer_layer->SetBounds(gfx::Size(10, 10));
    delegated_renderer_layer->SetContentBounds(gfx::Size(10, 10));
    delegated_renderer_layer->SetDrawsContent(true);
    gfx::Transform transform;
    transform.Translate(1.0, 1.0);
    delegated_renderer_layer->SetTransform(transform);

    ScopedPtrVector<RenderPass> delegated_render_passes;
    TestRenderPass* pass1 = addRenderPass(
        delegated_render_passes,
        RenderPass::Id(9, 6),
        gfx::Rect(6, 6, 6, 6),
        gfx::Transform());
    addQuad(pass1, gfx::Rect(0, 0, 6, 6), 33u);
    TestRenderPass* pass2 = addRenderPass(
        delegated_render_passes,
        RenderPass::Id(9, 7),
        gfx::Rect(7, 7, 7, 7),
        gfx::Transform());
    addQuad(pass2, gfx::Rect(0, 0, 7, 7), 22u);
    addRenderPassQuad(pass2, pass1);
    TestRenderPass* pass3 = addRenderPass(
        delegated_render_passes,
        RenderPass::Id(9, 8),
        gfx::Rect(0, 0, 8, 8),
        gfx::Transform());
    addRenderPassQuad(pass3, pass2);
    delegated_renderer_layer->SetFrameDataForRenderPasses(
        &delegated_render_passes);

    // The RenderPasses should be taken by the layer.
    EXPECT_EQ(0u, delegated_render_passes.size());

    root_layer_ = root_layer.get();
    layer_before_ = layer_before.get();
    layer_after_ = layer_after.get();
    delegated_renderer_layer_ = delegated_renderer_layer.get();

    // Force the delegated RenderPasses to come before the RenderPass from
    // layer_after.
    layer_after->AddChild(delegated_renderer_layer.PassAs<LayerImpl>());
    root_layer->AddChild(layer_after.Pass());

    // Get the RenderPass generated by layer_before to come before the delegated
    // RenderPasses.
    root_layer->AddChild(layer_before.Pass());
    host_impl_->active_tree()->SetRootLayer(root_layer.Pass());
  }

 protected:
  LayerImpl* root_layer_;
  LayerImpl* layer_before_;
  LayerImpl* layer_after_;
  DelegatedRendererLayerImpl* delegated_renderer_layer_;
};

TEST_F(DelegatedRendererLayerImplTestSimple, AddsContributingRenderPasses) {
  LayerTreeHostImpl::FrameData frame;
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));

  // Each non-DelegatedRendererLayer added one RenderPass. The
  // DelegatedRendererLayer added two contributing passes.
  ASSERT_EQ(5u, frame.render_passes.size());

  // The DelegatedRendererLayer should have added its contributing RenderPasses
  // to the frame.
  EXPECT_EQ(4, frame.render_passes[1]->id.layer_id);
  EXPECT_EQ(1, frame.render_passes[1]->id.index);
  EXPECT_EQ(4, frame.render_passes[2]->id.layer_id);
  EXPECT_EQ(2, frame.render_passes[2]->id.index);
  // And all other RenderPasses should be non-delegated.
  EXPECT_NE(4, frame.render_passes[0]->id.layer_id);
  EXPECT_EQ(0, frame.render_passes[0]->id.index);
  EXPECT_NE(4, frame.render_passes[3]->id.layer_id);
  EXPECT_EQ(0, frame.render_passes[3]->id.index);
  EXPECT_NE(4, frame.render_passes[4]->id.layer_id);
  EXPECT_EQ(0, frame.render_passes[4]->id.index);

  // The DelegatedRendererLayer should have added its RenderPasses to the frame
  // in order.
  EXPECT_EQ(gfx::Rect(6, 6, 6, 6).ToString(),
            frame.render_passes[1]->output_rect.ToString());
  EXPECT_EQ(gfx::Rect(7, 7, 7, 7).ToString(),
            frame.render_passes[2]->output_rect.ToString());

  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  host_impl_->DidDrawAllLayers(frame);
}

TEST_F(DelegatedRendererLayerImplTestSimple,
       AddsQuadsToContributingRenderPasses) {
  LayerTreeHostImpl::FrameData frame;
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));

  // Each non-DelegatedRendererLayer added one RenderPass. The
  // DelegatedRendererLayer added two contributing passes.
  ASSERT_EQ(5u, frame.render_passes.size());

  // The DelegatedRendererLayer should have added its contributing RenderPasses
  // to the frame.
  EXPECT_EQ(4, frame.render_passes[1]->id.layer_id);
  EXPECT_EQ(1, frame.render_passes[1]->id.index);
  EXPECT_EQ(4, frame.render_passes[2]->id.layer_id);
  EXPECT_EQ(2, frame.render_passes[2]->id.index);

  // The DelegatedRendererLayer should have added copies of its quads to
  // contributing RenderPasses.
  ASSERT_EQ(1u, frame.render_passes[1]->quad_list.size());
  EXPECT_EQ(gfx::Rect(0, 0, 6, 6).ToString(),
            frame.render_passes[1]->quad_list[0]->rect.ToString());

  // Verify it added the right quads.
  ASSERT_EQ(2u, frame.render_passes[2]->quad_list.size());
  EXPECT_EQ(gfx::Rect(0, 0, 7, 7).ToString(),
            frame.render_passes[2]->quad_list[0]->rect.ToString());
  EXPECT_EQ(gfx::Rect(6, 6, 6, 6).ToString(),
            frame.render_passes[2]->quad_list[1]->rect.ToString());
  ASSERT_EQ(1u, frame.render_passes[1]->quad_list.size());
  EXPECT_EQ(gfx::Rect(0, 0, 6, 6).ToString(),
            frame.render_passes[1]->quad_list[0]->rect.ToString());

  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  host_impl_->DidDrawAllLayers(frame);
}

TEST_F(DelegatedRendererLayerImplTestSimple, AddsQuadsToTargetRenderPass) {
  LayerTreeHostImpl::FrameData frame;
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));

  // Each non-DelegatedRendererLayer added one RenderPass. The
  // DelegatedRendererLayer added two contributing passes.
  ASSERT_EQ(5u, frame.render_passes.size());

  // The layer's target is the RenderPass from m_layer_after.
  EXPECT_EQ(RenderPass::Id(3, 0), frame.render_passes[3]->id);

  // The DelegatedRendererLayer should have added copies of quads in its root
  // RenderPass to its target RenderPass. The m_layer_after also adds one quad.
  ASSERT_EQ(2u, frame.render_passes[3]->quad_list.size());

  // Verify it added the right quads.
  EXPECT_EQ(gfx::Rect(7, 7, 7, 7).ToString(),
            frame.render_passes[3]->quad_list[0]->rect.ToString());

  // Its target layer should have a quad as well.
  EXPECT_EQ(gfx::Rect(0, 0, 15, 15).ToString(),
            frame.render_passes[3]->quad_list[1]->rect.ToString());

  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  host_impl_->DidDrawAllLayers(frame);
}

TEST_F(DelegatedRendererLayerImplTestSimple,
       QuadsFromRootRenderPassAreModifiedForTheTarget) {
  LayerTreeHostImpl::FrameData frame;
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));

  // Each non-DelegatedRendererLayer added one RenderPass. The
  // DelegatedRendererLayer added two contributing passes.
  ASSERT_EQ(5u, frame.render_passes.size());

  // The DelegatedRendererLayer is at position 3,3 compared to its target, and
  // has a translation transform of 1,1. So its root RenderPass' quads should
  // all be transformed by that combined amount.
  // The DelegatedRendererLayer has a size of 10x10, but the root delegated
  // RenderPass has a size of 8x8, so any quads should be scaled by 10/8.
  gfx::Transform transform;
  transform.Translate(4.0, 4.0);
  transform.Scale(10.0 / 8.0, 10.0 / 8.0);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      transform, frame.render_passes[3]->quad_list[0]->quadTransform());

  // Quads from non-root RenderPasses should not be shifted though.
  ASSERT_EQ(2u, frame.render_passes[2]->quad_list.size());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      gfx::Transform(), frame.render_passes[2]->quad_list[0]->quadTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      gfx::Transform(), frame.render_passes[2]->quad_list[1]->quadTransform());
  ASSERT_EQ(1u, frame.render_passes[1]->quad_list.size());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      gfx::Transform(), frame.render_passes[1]->quad_list[0]->quadTransform());

  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  host_impl_->DidDrawAllLayers(frame);
}

TEST_F(DelegatedRendererLayerImplTestSimple, DoesNotOwnARenderSurface) {
  LayerTreeHostImpl::FrameData frame;
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));

  // If the DelegatedRendererLayer is axis aligned and has opacity 1, then it
  // has no need to be a renderSurface for the quads it carries.
  EXPECT_FALSE(delegated_renderer_layer_->render_surface());

  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  host_impl_->DidDrawAllLayers(frame);
}

TEST_F(DelegatedRendererLayerImplTestSimple, DoesOwnARenderSurfaceForOpacity) {
  delegated_renderer_layer_->SetOpacity(0.5f);

  LayerTreeHostImpl::FrameData frame;
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));

  // This test case has quads from multiple layers in the delegated renderer, so
  // if the DelegatedRendererLayer has opacity < 1, it should end up with a
  // render surface.
  EXPECT_TRUE(delegated_renderer_layer_->render_surface());

  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  host_impl_->DidDrawAllLayers(frame);
}

TEST_F(DelegatedRendererLayerImplTestSimple,
       DoesOwnARenderSurfaceForTransform) {
  gfx::Transform rotation;
  rotation.RotateAboutZAxis(30.0);
  delegated_renderer_layer_->SetTransform(rotation);

  LayerTreeHostImpl::FrameData frame;
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));

  // This test case has quads from multiple layers in the delegated renderer, so
  // if the DelegatedRendererLayer has opacity < 1, it should end up with a
  // render surface.
  EXPECT_TRUE(delegated_renderer_layer_->render_surface());

  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  host_impl_->DidDrawAllLayers(frame);
}

class DelegatedRendererLayerImplTestOwnSurface
    : public DelegatedRendererLayerImplTestSimple {
 public:
  DelegatedRendererLayerImplTestOwnSurface()
      : DelegatedRendererLayerImplTestSimple() {
    delegated_renderer_layer_->SetForceRenderSurface(true);
  }
};

TEST_F(DelegatedRendererLayerImplTestOwnSurface, AddsRenderPasses) {
  LayerTreeHostImpl::FrameData frame;
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));

  // Each non-DelegatedRendererLayer added one RenderPass. The
  // DelegatedRendererLayer added two contributing passes and its owned surface
  // added one pass.
  ASSERT_EQ(6u, frame.render_passes.size());

  // The DelegatedRendererLayer should have added its contributing RenderPasses
  // to the frame.
  EXPECT_EQ(4, frame.render_passes[1]->id.layer_id);
  EXPECT_EQ(1, frame.render_passes[1]->id.index);
  EXPECT_EQ(4, frame.render_passes[2]->id.layer_id);
  EXPECT_EQ(2, frame.render_passes[2]->id.index);
  // The DelegatedRendererLayer should have added a RenderPass for its surface
  // to the frame.
  EXPECT_EQ(4, frame.render_passes[1]->id.layer_id);
  EXPECT_EQ(0, frame.render_passes[3]->id.index);
  // And all other RenderPasses should be non-delegated.
  EXPECT_NE(4, frame.render_passes[0]->id.layer_id);
  EXPECT_EQ(0, frame.render_passes[0]->id.index);
  EXPECT_NE(4, frame.render_passes[4]->id.layer_id);
  EXPECT_EQ(0, frame.render_passes[4]->id.index);
  EXPECT_NE(4, frame.render_passes[5]->id.layer_id);
  EXPECT_EQ(0, frame.render_passes[5]->id.index);

  // The DelegatedRendererLayer should have added its RenderPasses to the frame
  // in order.
  EXPECT_EQ(gfx::Rect(6, 6, 6, 6).ToString(),
            frame.render_passes[1]->output_rect.ToString());
  EXPECT_EQ(gfx::Rect(7, 7, 7, 7).ToString(),
            frame.render_passes[2]->output_rect.ToString());

  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  host_impl_->DidDrawAllLayers(frame);
}

TEST_F(DelegatedRendererLayerImplTestOwnSurface,
       AddsQuadsToContributingRenderPasses) {
  LayerTreeHostImpl::FrameData frame;
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));

  // Each non-DelegatedRendererLayer added one RenderPass. The
  // DelegatedRendererLayer added two contributing passes and its owned surface
  // added one pass.
  ASSERT_EQ(6u, frame.render_passes.size());

  // The DelegatedRendererLayer should have added its contributing RenderPasses
  // to the frame.
  EXPECT_EQ(4, frame.render_passes[1]->id.layer_id);
  EXPECT_EQ(1, frame.render_passes[1]->id.index);
  EXPECT_EQ(4, frame.render_passes[2]->id.layer_id);
  EXPECT_EQ(2, frame.render_passes[2]->id.index);

  // The DelegatedRendererLayer should have added copies of its quads to
  // contributing RenderPasses.
  ASSERT_EQ(1u, frame.render_passes[1]->quad_list.size());
  EXPECT_EQ(gfx::Rect(0, 0, 6, 6).ToString(),
            frame.render_passes[1]->quad_list[0]->rect.ToString());

  // Verify it added the right quads.
  ASSERT_EQ(2u, frame.render_passes[2]->quad_list.size());
  EXPECT_EQ(gfx::Rect(0, 0, 7, 7).ToString(),
            frame.render_passes[2]->quad_list[0]->rect.ToString());
  EXPECT_EQ(gfx::Rect(6, 6, 6, 6).ToString(),
            frame.render_passes[2]->quad_list[1]->rect.ToString());
  ASSERT_EQ(1u, frame.render_passes[1]->quad_list.size());
  EXPECT_EQ(gfx::Rect(0, 0, 6, 6).ToString(),
            frame.render_passes[1]->quad_list[0]->rect.ToString());

  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  host_impl_->DidDrawAllLayers(frame);
}

TEST_F(DelegatedRendererLayerImplTestOwnSurface, AddsQuadsToTargetRenderPass) {
  LayerTreeHostImpl::FrameData frame;
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));

  // Each non-DelegatedRendererLayer added one RenderPass. The
  // DelegatedRendererLayer added two contributing passes and its owned surface
  // added one pass.
  ASSERT_EQ(6u, frame.render_passes.size());

  // The layer's target is the RenderPass owned by itself.
  EXPECT_EQ(RenderPass::Id(4, 0), frame.render_passes[3]->id);

  // The DelegatedRendererLayer should have added copies of quads in its root
  // RenderPass to its target RenderPass.
  // The m_layer_after also adds one quad.
  ASSERT_EQ(1u, frame.render_passes[3]->quad_list.size());

  // Verify it added the right quads.
  EXPECT_EQ(gfx::Rect(7, 7, 7, 7).ToString(),
            frame.render_passes[3]->quad_list[0]->rect.ToString());

  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  host_impl_->DidDrawAllLayers(frame);
}

TEST_F(DelegatedRendererLayerImplTestOwnSurface,
       QuadsFromRootRenderPassAreNotModifiedForTheTarget) {
  LayerTreeHostImpl::FrameData frame;
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));

  // Each non-DelegatedRendererLayer added one RenderPass. The
  // DelegatedRendererLayer added two contributing passes and its owned surface
  // added one pass.
  ASSERT_EQ(6u, frame.render_passes.size());

  // Because the DelegatedRendererLayer owns a RenderSurfaceImpl, its root
  // RenderPass' quads do not need to be translated at all. However, they are
  // scaled from the frame's size (8x8) to the layer's bounds (10x10).
  gfx::Transform transform;
  transform.Scale(10.0 / 8.0, 10.0 / 8.0);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      transform, frame.render_passes[3]->quad_list[0]->quadTransform());

  // Quads from non-root RenderPasses should not be shifted either.
  ASSERT_EQ(2u, frame.render_passes[2]->quad_list.size());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      gfx::Transform(), frame.render_passes[2]->quad_list[0]->quadTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      gfx::Transform(), frame.render_passes[2]->quad_list[1]->quadTransform());
  ASSERT_EQ(1u, frame.render_passes[1]->quad_list.size());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      gfx::Transform(), frame.render_passes[1]->quad_list[0]->quadTransform());

  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  host_impl_->DidDrawAllLayers(frame);
}

class DelegatedRendererLayerImplTestTransform
    : public DelegatedRendererLayerImplTest {
 public:
  void SetUpTest() {
    scoped_ptr<LayerImpl> root_layer = LayerImpl::Create(
        host_impl_->active_tree(), 1);
    scoped_ptr<FakeDelegatedRendererLayerImpl> delegated_renderer_layer =
        FakeDelegatedRendererLayerImpl::Create(host_impl_->active_tree(), 2);

    host_impl_->SetViewportSize(gfx::Size(100, 100), gfx::Size(100, 100));
    root_layer->SetBounds(gfx::Size(100, 100));

    delegated_renderer_layer->SetPosition(gfx::Point(20, 20));
    delegated_renderer_layer->SetBounds(gfx::Size(30, 30)); 
    delegated_renderer_layer->SetContentBounds(gfx::Size(30, 30));
    delegated_renderer_layer->SetDrawsContent(true);
    gfx::Transform transform;
    transform.Scale(2.0, 2.0);
    transform.Translate(8.0, 8.0);
    delegated_renderer_layer->SetTransform(transform);

    ScopedPtrVector<RenderPass> delegated_render_passes;

    gfx::Size child_pass_content_bounds(7, 7);
    gfx::Rect child_pass_rect(20, 20, 7, 7);
    gfx::Transform child_pass_transform;
    child_pass_transform.Scale(0.8, 0.8);
    child_pass_transform.Translate(9.0, 9.0);
    gfx::Rect child_pass_clip_rect(21, 21, 3, 3);
    bool child_pass_clipped = false;

    {
      TestRenderPass* pass = addRenderPass(
          delegated_render_passes,
          RenderPass::Id(10, 7),
          child_pass_rect,
          gfx::Transform());
      MockQuadCuller quad_sink(&pass->quad_list, &pass->shared_quad_state_list);
      AppendQuadsData data(pass->id);
      SharedQuadState* shared_quad_state = quad_sink.UseSharedQuadState(
          SharedQuadState::Create());
      shared_quad_state->SetAll(
          child_pass_transform,
          child_pass_content_bounds,
          child_pass_rect,
          child_pass_clip_rect,
          child_pass_clipped,
          1.f);

      scoped_ptr<SolidColorDrawQuad> color_quad;
      color_quad = SolidColorDrawQuad::Create();
      color_quad->SetNew(shared_quad_state, gfx::Rect(20, 20, 3, 7), 1u);
      quad_sink.Append(color_quad.PassAs<DrawQuad>(), &data);

      color_quad = SolidColorDrawQuad::Create();
      color_quad->SetNew(shared_quad_state, gfx::Rect(23, 20, 4, 7), 1u);
      quad_sink.Append(color_quad.PassAs<DrawQuad>(), &data);
    }

    gfx::Size root_pass_content_bounds(50, 50);
    gfx::Rect root_pass_rect(0, 0, 50, 50);
    gfx::Transform root_pass_transform;
    root_pass_transform.Scale(1.5, 1.5);
    root_pass_transform.Translate(7.0, 7.0);
    gfx::Rect root_pass_clip_rect(10, 10, 35, 35);
    bool root_pass_clipped = root_delegated_render_pass_is_clipped_;

    TestRenderPass* pass = addRenderPass(
        delegated_render_passes,
        RenderPass::Id(9, 6),
        root_pass_rect,
        gfx::Transform());
    MockQuadCuller quad_sink(&pass->quad_list, &pass->shared_quad_state_list);
    AppendQuadsData data(pass->id);
    SharedQuadState* shared_quad_state =
        quad_sink.UseSharedQuadState(SharedQuadState::Create());
    shared_quad_state->SetAll(
        root_pass_transform,
        root_pass_content_bounds,
        root_pass_rect,
        root_pass_clip_rect,
        root_pass_clipped,
        1.f);

    scoped_ptr<RenderPassDrawQuad> render_pass_quad =
        RenderPassDrawQuad::Create();
    render_pass_quad->SetNew(
        shared_quad_state,
        gfx::Rect(5, 5, 7, 7),  // rect
        RenderPass::Id(10, 7),  // render_pass_id
        false,  // is_replica
        0,  // mask_resource_id
        child_pass_rect,  // contents_changed_since_last_frame
        gfx::RectF(),  // mask_uv_rect
        WebKit::WebFilterOperations(),  // filters
        skia::RefPtr<SkImageFilter>(),  // filter
        WebKit::WebFilterOperations());  // background_filters
    quad_sink.Append(render_pass_quad.PassAs<DrawQuad>(), &data);

    scoped_ptr<SolidColorDrawQuad> color_quad;
    color_quad = SolidColorDrawQuad::Create();
    color_quad->SetNew(shared_quad_state, gfx::Rect(0, 0, 10, 10), 1u);
    quad_sink.Append(color_quad.PassAs<DrawQuad>(), &data);

    color_quad = SolidColorDrawQuad::Create();
    color_quad->SetNew(shared_quad_state, gfx::Rect(0, 10, 10, 10), 2u);
    quad_sink.Append(color_quad.PassAs<DrawQuad>(), &data);

    color_quad = SolidColorDrawQuad::Create();
    color_quad->SetNew(shared_quad_state, gfx::Rect(10, 0, 10, 10), 3u);
    quad_sink.Append(color_quad.PassAs<DrawQuad>(), &data);

    color_quad = SolidColorDrawQuad::Create();
    color_quad->SetNew(shared_quad_state, gfx::Rect(10, 10, 10, 10), 4u);
    quad_sink.Append(color_quad.PassAs<DrawQuad>(), &data);

    delegated_renderer_layer->SetFrameDataForRenderPasses(
        &delegated_render_passes);

    // The RenderPasses should be taken by the layer.
    EXPECT_EQ(0u, delegated_render_passes.size());

    root_layer_ = root_layer.get();
    delegated_renderer_layer_ = delegated_renderer_layer.get();

    root_layer->AddChild(delegated_renderer_layer.PassAs<LayerImpl>());
    host_impl_->active_tree()->SetRootLayer(root_layer.Pass());
  }

  void VerifyRenderPasses(
      const LayerTreeHostImpl::FrameData& frame,
      size_t num_render_passes,
      const SharedQuadState** root_delegated_shared_quad_state,
      const SharedQuadState** contrib_delegated_shared_quad_state) {
    ASSERT_EQ(num_render_passes, frame.render_passes.size());
    // The contributing render pass in the DelegatedRendererLayer.
    EXPECT_EQ(2, frame.render_passes[0]->id.layer_id);
    EXPECT_EQ(1, frame.render_passes[0]->id.index);
    // The root render pass.
    EXPECT_EQ(1, frame.render_passes.back()->id.layer_id);
    EXPECT_EQ(0, frame.render_passes.back()->id.index);

    const QuadList& contrib_delegated_quad_list =
        frame.render_passes[0]->quad_list;
    ASSERT_EQ(2u, contrib_delegated_quad_list.size());

    const QuadList& root_delegated_quad_list =
        frame.render_passes[1]->quad_list;
    ASSERT_EQ(5u, root_delegated_quad_list.size());

    // All quads in a render pass should share the same state.
    *contrib_delegated_shared_quad_state =
        contrib_delegated_quad_list[0]->shared_quad_state;
    EXPECT_EQ(*contrib_delegated_shared_quad_state,
              contrib_delegated_quad_list[1]->shared_quad_state);

    *root_delegated_shared_quad_state =
        root_delegated_quad_list[0]->shared_quad_state;
    EXPECT_EQ(*root_delegated_shared_quad_state,
              root_delegated_quad_list[1]->shared_quad_state);
    EXPECT_EQ(*root_delegated_shared_quad_state,
              root_delegated_quad_list[2]->shared_quad_state);
    EXPECT_EQ(*root_delegated_shared_quad_state,
              root_delegated_quad_list[3]->shared_quad_state);
    EXPECT_EQ(*root_delegated_shared_quad_state,
              root_delegated_quad_list[4]->shared_quad_state);

    EXPECT_NE(*contrib_delegated_shared_quad_state,
              *root_delegated_shared_quad_state);
  }

 protected:
  LayerImpl* root_layer_;
  DelegatedRendererLayerImpl* delegated_renderer_layer_;
  bool root_delegated_render_pass_is_clipped_;
};

TEST_F(DelegatedRendererLayerImplTestTransform, QuadsUnclipped_NoSurface) {
  root_delegated_render_pass_is_clipped_ = false;
  SetUpTest();

  LayerTreeHostImpl::FrameData frame;
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));

  const SharedQuadState* root_delegated_shared_quad_state = NULL;
  const SharedQuadState* contrib_delegated_shared_quad_state = NULL;
  VerifyRenderPasses(
      frame,
      2,
      &root_delegated_shared_quad_state,
      &contrib_delegated_shared_quad_state);

  // When the quads don't have a clip of their own, the clip rect is set to
  // the drawableContentRect of the delegated renderer layer.
  EXPECT_EQ(gfx::Rect(21, 21, 60, 60).ToString(),
            root_delegated_shared_quad_state->clip_rect.ToString());

  // Even though the quads in the root pass have no clip of their own, they
  // inherit the clip rect from the delegated renderer layer if it does not
  // own a surface.
  EXPECT_TRUE(root_delegated_shared_quad_state->is_clipped);

  gfx::Transform expected;
  // This is the transform from the layer's space to its target.
  // The position (20) - the width / scale (30 / 2) = 20 - 15 = 5
  expected.Translate(5.0, 5.0);
  expected.Scale(2.0, 2.0);
  expected.Translate(8.0, 8.0);
  // The frame has size 50x50 but the layer's bounds are 30x30.
  expected.Scale(30.0 / 50.0, 30.0 / 50.0);
  // This is the transform within the source frame.
  expected.Scale(1.5, 1.5);
  expected.Translate(7.0, 7.0);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected, root_delegated_shared_quad_state->content_to_target_transform);

  // The contributing render pass should not be transformed from its input.
  EXPECT_EQ(gfx::Rect(21, 21, 3, 3).ToString(),
            contrib_delegated_shared_quad_state->clip_rect.ToString());
  EXPECT_FALSE(contrib_delegated_shared_quad_state->is_clipped);
  expected.MakeIdentity();
  expected.Scale(0.8, 0.8);
  expected.Translate(9.0, 9.0);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected,
      contrib_delegated_shared_quad_state->content_to_target_transform);

  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  host_impl_->DidDrawAllLayers(frame);
}

TEST_F(DelegatedRendererLayerImplTestTransform, QuadsClipped_NoSurface) {
  root_delegated_render_pass_is_clipped_ = true;
  SetUpTest();

  LayerTreeHostImpl::FrameData frame;
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));

  const SharedQuadState* root_delegated_shared_quad_state = NULL;
  const SharedQuadState* contrib_delegated_shared_quad_state = NULL;
  VerifyRenderPasses(
      frame,
      2,
      &root_delegated_shared_quad_state,
      &contrib_delegated_shared_quad_state);

  // Since the quads have a clip_rect it should be modified by delegated
  // renderer layer's drawTransform.
  // The position of the resulting clip_rect is:
  // (clip rect position (10) * scale to layer (30/50) + translate (8)) *
  //     layer scale (2) + layer position (20) = 48
  // But the layer is centered, so: 48 - (width / 2) = 48 - 30 / 2 = 33
  //
  // The size is 35x35 scaled to fit inside the layer's bounds at 30x30 from
  // a frame at 50x50: 35 * 2 (layer's scale) * 30 / 50 = 42.
  EXPECT_EQ(gfx::Rect(33, 33, 42, 42).ToString(),
            root_delegated_shared_quad_state->clip_rect.ToString());

  // The quads had a clip and it should be preserved.
  EXPECT_TRUE(root_delegated_shared_quad_state->is_clipped);

  gfx::Transform expected;
  // This is the transform from the layer's space to its target.
  // The position (20) - the width / scale (30 / 2) = 20 - 15 = 5
  expected.Translate(5.0, 5.0);
  expected.Scale(2.0, 2.0);
  expected.Translate(8.0, 8.0);
  // The frame has size 50x50 but the layer's bounds are 30x30.
  expected.Scale(30.0 / 50.0, 30.0 / 50.0);
  // This is the transform within the source frame.
  expected.Scale(1.5, 1.5);
  expected.Translate(7.0, 7.0);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected, root_delegated_shared_quad_state->content_to_target_transform);

  // The contributing render pass should not be transformed from its input.
  EXPECT_EQ(gfx::Rect(21, 21, 3, 3).ToString(),
            contrib_delegated_shared_quad_state->clip_rect.ToString());
  EXPECT_FALSE(contrib_delegated_shared_quad_state->is_clipped);
  expected.MakeIdentity();
  expected.Scale(0.8, 0.8);
  expected.Translate(9.0, 9.0);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected,
      contrib_delegated_shared_quad_state->content_to_target_transform);

  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  host_impl_->DidDrawAllLayers(frame);
}

TEST_F(DelegatedRendererLayerImplTestTransform, QuadsUnclipped_Surface) {
  root_delegated_render_pass_is_clipped_ = false;
  SetUpTest();

  delegated_renderer_layer_->SetForceRenderSurface(true);

  LayerTreeHostImpl::FrameData frame;
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));

  const SharedQuadState* root_delegated_shared_quad_state = NULL;
  const SharedQuadState* contrib_delegated_shared_quad_state = NULL;
  VerifyRenderPasses(
      frame,
      3,
      &root_delegated_shared_quad_state,
      &contrib_delegated_shared_quad_state);

  // When the layer owns a surface, then its position and translation are not
  // a part of its draw transform.
  // The position of the resulting clip_rect is:
  // (clip rect position (10) * scale to layer (30/50)) * layer scale (2) = 12
  // The size is 35x35 scaled to fit inside the layer's bounds at 30x30 from
  // a frame at 50x50: 35 * 2 (layer's scale) * 30 / 50 = 42.
  EXPECT_EQ(gfx::Rect(12, 12, 42, 42).ToString(),
            root_delegated_shared_quad_state->clip_rect.ToString());

  // Since the layer owns a surface it doesn't need to clip its quads, so
  // unclipped quads remain unclipped.
  EXPECT_FALSE(root_delegated_shared_quad_state->is_clipped);

  gfx::Transform expected;
  expected.Scale(2.0, 2.0);
  // The frame has size 50x50 but the layer's bounds are 30x30.
  expected.Scale(30.0 / 50.0, 30.0 / 50.0);
  // This is the transform within the source frame.
  expected.Scale(1.5, 1.5);
  expected.Translate(7.0, 7.0);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected, root_delegated_shared_quad_state->content_to_target_transform);

  // The contributing render pass should not be transformed from its input.
  EXPECT_EQ(gfx::Rect(21, 21, 3, 3).ToString(),
            contrib_delegated_shared_quad_state->clip_rect.ToString());
  EXPECT_FALSE(contrib_delegated_shared_quad_state->is_clipped);
  expected.MakeIdentity();
  expected.Scale(0.8, 0.8);
  expected.Translate(9.0, 9.0);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected,
      contrib_delegated_shared_quad_state->content_to_target_transform);

  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  host_impl_->DidDrawAllLayers(frame);
}

TEST_F(DelegatedRendererLayerImplTestTransform, QuadsClipped_Surface) {
  root_delegated_render_pass_is_clipped_ = true;
  SetUpTest();

  delegated_renderer_layer_->SetForceRenderSurface(true);

  LayerTreeHostImpl::FrameData frame;
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));

  const SharedQuadState* root_delegated_shared_quad_state = NULL;
  const SharedQuadState* contrib_delegated_shared_quad_state = NULL;
  VerifyRenderPasses(
      frame,
      3,
      &root_delegated_shared_quad_state,
      &contrib_delegated_shared_quad_state);

  // When the layer owns a surface, then its position and translation are not
  // a part of its draw transform.
  // The position of the resulting clip_rect is:
  // (clip rect position (10) * scale to layer (30/50)) * layer scale (2) = 12
  // The size is 35x35 scaled to fit inside the layer's bounds at 30x30 from
  // a frame at 50x50: 35 * 2 (layer's scale) * 30 / 50 = 42.
  EXPECT_EQ(gfx::Rect(12, 12, 42, 42).ToString(),
            root_delegated_shared_quad_state->clip_rect.ToString());

  // The quads had a clip and it should be preserved.
  EXPECT_TRUE(root_delegated_shared_quad_state->is_clipped);

  gfx::Transform expected;
  expected.Scale(2.0, 2.0);
  // The frame has size 50x50 but the layer's bounds are 30x30.
  expected.Scale(30.0 / 50.0, 30.0 / 50.0);
  // This is the transform within the source frame.
  expected.Scale(1.5, 1.5);
  expected.Translate(7.0, 7.0);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected, root_delegated_shared_quad_state->content_to_target_transform);

  // The contributing render pass should not be transformed from its input.
  EXPECT_EQ(gfx::Rect(21, 21, 3, 3).ToString(),
            contrib_delegated_shared_quad_state->clip_rect.ToString());
  EXPECT_FALSE(contrib_delegated_shared_quad_state->is_clipped);
  expected.MakeIdentity();
  expected.Scale(0.8, 0.8);
  expected.Translate(9.0, 9.0);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected,
      contrib_delegated_shared_quad_state->content_to_target_transform);

  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  host_impl_->DidDrawAllLayers(frame);
}

class DelegatedRendererLayerImplTestClip
    : public DelegatedRendererLayerImplTest {
 public:
  void SetUpTest() {
    scoped_ptr<LayerImpl> root_layer =
        LayerImpl::Create(host_impl_->active_tree(), 1);
    scoped_ptr<FakeDelegatedRendererLayerImpl> delegated_renderer_layer =
        FakeDelegatedRendererLayerImpl::Create(host_impl_->active_tree(), 2);
    scoped_ptr<LayerImpl> clip_layer =
        LayerImpl::Create(host_impl_->active_tree(), 3);
    scoped_ptr<LayerImpl> origin_layer =
        LayerImpl::Create(host_impl_->active_tree(), 4);

    host_impl_->SetViewportSize(gfx::Size(100, 100), gfx::Size(100, 100));
    root_layer->SetBounds(gfx::Size(100, 100));

    delegated_renderer_layer->SetPosition(gfx::Point(20, 20));
    delegated_renderer_layer->SetBounds(gfx::Size(50, 50));
    delegated_renderer_layer->SetContentBounds(gfx::Size(50, 50));
    delegated_renderer_layer->SetDrawsContent(true);

    ScopedPtrVector<RenderPass> delegated_render_passes;

    gfx::Size child_pass_content_bounds(7, 7);
    gfx::Rect child_pass_rect(20, 20, 7, 7);
    gfx::Transform child_pass_transform;
    gfx::Rect child_pass_clip_rect(21, 21, 3, 3);
    bool child_pass_clipped = false;

    {
      TestRenderPass* pass = addRenderPass(
          delegated_render_passes,
          RenderPass::Id(10, 7),
          child_pass_rect,
          gfx::Transform());
      MockQuadCuller quad_sink(&pass->quad_list, &pass->shared_quad_state_list);
      AppendQuadsData data(pass->id);
      SharedQuadState* shared_quad_state =
          quad_sink.UseSharedQuadState(SharedQuadState::Create());
      shared_quad_state->SetAll(
          child_pass_transform,
          child_pass_content_bounds,
          child_pass_rect,
          child_pass_clip_rect,
          child_pass_clipped,
          1.f);

      scoped_ptr<SolidColorDrawQuad> color_quad;
      color_quad = SolidColorDrawQuad::Create();
      color_quad->SetNew(shared_quad_state, gfx::Rect(20, 20, 3, 7), 1u);
      quad_sink.Append(color_quad.PassAs<DrawQuad>(), &data);

      color_quad = SolidColorDrawQuad::Create();
      color_quad->SetNew(shared_quad_state, gfx::Rect(23, 20, 4, 7), 1u);
      quad_sink.Append(color_quad.PassAs<DrawQuad>(), &data);
    }

    gfx::Size root_pass_content_bounds(50, 50);
    gfx::Rect root_pass_rect(0, 0, 50, 50);
    gfx::Transform root_pass_transform;
    gfx::Rect root_pass_clip_rect(5, 5, 40, 40);
    bool root_pass_clipped = root_delegated_render_pass_is_clipped_;

    TestRenderPass* pass = addRenderPass(
        delegated_render_passes,
        RenderPass::Id(9, 6),
        root_pass_rect,
        gfx::Transform());
    MockQuadCuller quad_sink(&pass->quad_list, &pass->shared_quad_state_list);
    AppendQuadsData data(pass->id);
    SharedQuadState* shared_quad_state =
        quad_sink.UseSharedQuadState(SharedQuadState::Create());
    shared_quad_state->SetAll(root_pass_transform,
                              root_pass_content_bounds,
                              root_pass_rect,
                              root_pass_clip_rect,
                              root_pass_clipped,
                              1.f);

    scoped_ptr<RenderPassDrawQuad> render_pass_quad =
        RenderPassDrawQuad::Create();
    render_pass_quad->SetNew(
        shared_quad_state,
        gfx::Rect(5, 5, 7, 7),  // rect
        RenderPass::Id(10, 7),  // render_pass_id
        false,  // is_replica
        0,  // mask_resource_id
        child_pass_rect,  // contents_changed_since_last_frame
        gfx::RectF(),  // mask_uv_rect
        WebKit::WebFilterOperations(),  // filters
        skia::RefPtr<SkImageFilter>(),  // filter
        WebKit::WebFilterOperations());  // background_filters
    quad_sink.Append(render_pass_quad.PassAs<DrawQuad>(), &data);

    scoped_ptr<SolidColorDrawQuad> color_quad;
    color_quad = SolidColorDrawQuad::Create();
    color_quad->SetNew(shared_quad_state, gfx::Rect(0, 0, 10, 10), 1u);
    quad_sink.Append(color_quad.PassAs<DrawQuad>(), &data);

    color_quad = SolidColorDrawQuad::Create();
    color_quad->SetNew(shared_quad_state, gfx::Rect(0, 10, 10, 10), 2u);
    quad_sink.Append(color_quad.PassAs<DrawQuad>(), &data);

    color_quad = SolidColorDrawQuad::Create();
    color_quad->SetNew(shared_quad_state, gfx::Rect(10, 0, 10, 10), 3u);
    quad_sink.Append(color_quad.PassAs<DrawQuad>(), &data);

    color_quad = SolidColorDrawQuad::Create();
    color_quad->SetNew(shared_quad_state, gfx::Rect(10, 10, 10, 10), 4u);
    quad_sink.Append(color_quad.PassAs<DrawQuad>(), &data);

    delegated_renderer_layer->SetFrameDataForRenderPasses(
        &delegated_render_passes);

    // The RenderPasses should be taken by the layer.
    EXPECT_EQ(0u, delegated_render_passes.size());

    root_layer_ = root_layer.get();
    delegated_renderer_layer_ = delegated_renderer_layer.get();

    if (clip_delegated_renderer_layer_) {
      gfx::Rect clip_rect(21, 27, 23, 21);

      clip_layer->SetPosition(clip_rect.origin());
      clip_layer->SetBounds(clip_rect.size());
      clip_layer->SetContentBounds(clip_rect.size());
      clip_layer->SetMasksToBounds(true);

      origin_layer->SetPosition(
          gfx::PointAtOffsetFromOrigin(-clip_rect.OffsetFromOrigin()));

      origin_layer->AddChild(delegated_renderer_layer.PassAs<LayerImpl>());
      clip_layer->AddChild(origin_layer.Pass());
      root_layer->AddChild(clip_layer.Pass());
    } else {
      root_layer->AddChild(delegated_renderer_layer.PassAs<LayerImpl>());
    }

    host_impl_->active_tree()->SetRootLayer(root_layer.Pass());
  }

 protected:
  LayerImpl* root_layer_;
  DelegatedRendererLayerImpl* delegated_renderer_layer_;
  bool root_delegated_render_pass_is_clipped_;
  bool clip_delegated_renderer_layer_;
};

TEST_F(DelegatedRendererLayerImplTestClip,
       QuadsUnclipped_LayerUnclipped_NoSurface) {
  root_delegated_render_pass_is_clipped_ = false;
  clip_delegated_renderer_layer_ = false;
  SetUpTest();

  LayerTreeHostImpl::FrameData frame;
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));

  ASSERT_EQ(2u, frame.render_passes.size());
  const QuadList& contrib_delegated_quad_list =
      frame.render_passes[0]->quad_list;
  ASSERT_EQ(2u, contrib_delegated_quad_list.size());
  const QuadList& root_delegated_quad_list = frame.render_passes[1]->quad_list;
  ASSERT_EQ(5u, root_delegated_quad_list.size());
  const SharedQuadState* root_delegated_shared_quad_state =
      root_delegated_quad_list[0]->shared_quad_state;
  const SharedQuadState* contrib_delegated_shared_quad_state =
      contrib_delegated_quad_list[0]->shared_quad_state;

  // When the quads don't have a clip of their own, the clip rect is set to
  // the drawableContentRect of the delegated renderer layer.
  EXPECT_EQ(gfx::Rect(20, 20, 50, 50).ToString(),
            root_delegated_shared_quad_state->clip_rect.ToString());
  // Quads are clipped to the delegated renderer layer.
  EXPECT_TRUE(root_delegated_shared_quad_state->is_clipped);

  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  host_impl_->DidDrawAllLayers(frame);
}

TEST_F(DelegatedRendererLayerImplTestClip,
       QuadsClipped_LayerUnclipped_NoSurface) {
  root_delegated_render_pass_is_clipped_ = true;
  clip_delegated_renderer_layer_ = false;
  SetUpTest();

  LayerTreeHostImpl::FrameData frame;
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));

  ASSERT_EQ(2u, frame.render_passes.size());
  const QuadList& contrib_delegated_quad_list =
      frame.render_passes[0]->quad_list;
  ASSERT_EQ(2u, contrib_delegated_quad_list.size());
  const QuadList& root_delegated_quad_list =
      frame.render_passes[1]->quad_list;
  ASSERT_EQ(5u, root_delegated_quad_list.size());
  const SharedQuadState* root_delegated_shared_quad_state =
      root_delegated_quad_list[0]->shared_quad_state;
  const SharedQuadState* contrib_delegated_shared_quad_state =
      contrib_delegated_quad_list[0]->shared_quad_state;

  // When the quads have a clip of their own, it is used.
  EXPECT_EQ(gfx::Rect(25, 25, 40, 40).ToString(),
            root_delegated_shared_quad_state->clip_rect.ToString());
  // Quads came with a clip rect.
  EXPECT_TRUE(root_delegated_shared_quad_state->is_clipped);

  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  host_impl_->DidDrawAllLayers(frame);
}

TEST_F(DelegatedRendererLayerImplTestClip,
       QuadsUnclipped_LayerClipped_NoSurface) {
  root_delegated_render_pass_is_clipped_ = false;
  clip_delegated_renderer_layer_ = true;
  SetUpTest();

  LayerTreeHostImpl::FrameData frame;
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));

  ASSERT_EQ(2u, frame.render_passes.size());
  const QuadList& contrib_delegated_quad_list =
      frame.render_passes[0]->quad_list;
  ASSERT_EQ(2u, contrib_delegated_quad_list.size());
  const QuadList& root_delegated_quad_list = frame.render_passes[1]->quad_list;
  ASSERT_EQ(5u, root_delegated_quad_list.size());
  const SharedQuadState* root_delegated_shared_quad_state =
      root_delegated_quad_list[0]->shared_quad_state;
  const SharedQuadState* contrib_delegated_shared_quad_state =
      contrib_delegated_quad_list[0]->shared_quad_state;

  // When the quads don't have a clip of their own, the clip rect is set to
  // the drawableContentRect of the delegated renderer layer. When the layer
  // is clipped, that should be seen in the quads' clip_rect.
  EXPECT_EQ(gfx::Rect(21, 27, 23, 21).ToString(),
            root_delegated_shared_quad_state->clip_rect.ToString());
  // Quads are clipped to the delegated renderer layer.
  EXPECT_TRUE(root_delegated_shared_quad_state->is_clipped);

  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  host_impl_->DidDrawAllLayers(frame);
}

TEST_F(DelegatedRendererLayerImplTestClip,
       QuadsClipped_LayerClipped_NoSurface) {
  root_delegated_render_pass_is_clipped_ = true;
  clip_delegated_renderer_layer_ = true;
  SetUpTest();

  LayerTreeHostImpl::FrameData frame;
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));

  ASSERT_EQ(2u, frame.render_passes.size());
  const QuadList& contrib_delegated_quad_list =
      frame.render_passes[0]->quad_list;
  ASSERT_EQ(2u, contrib_delegated_quad_list.size());
  const QuadList& root_delegated_quad_list = frame.render_passes[1]->quad_list;
  ASSERT_EQ(5u, root_delegated_quad_list.size());
  const SharedQuadState* root_delegated_shared_quad_state =
      root_delegated_quad_list[0]->shared_quad_state;
  const SharedQuadState* contrib_delegated_shared_quad_state =
      contrib_delegated_quad_list[0]->shared_quad_state;

  // When the quads have a clip of their own, it is used, but it is
  // combined with the clip rect of the delegated renderer layer.
  EXPECT_EQ(gfx::Rect(25, 27, 19, 21).ToString(),
            root_delegated_shared_quad_state->clip_rect.ToString());
  // Quads came with a clip rect.
  EXPECT_TRUE(root_delegated_shared_quad_state->is_clipped);

  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  host_impl_->DidDrawAllLayers(frame);
}

TEST_F(DelegatedRendererLayerImplTestClip,
       QuadsUnclipped_LayerUnclipped_Surface) {
  root_delegated_render_pass_is_clipped_ = false;
  clip_delegated_renderer_layer_ = false;
  SetUpTest();

  delegated_renderer_layer_->SetForceRenderSurface(true);

  LayerTreeHostImpl::FrameData frame;
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));

  ASSERT_EQ(3u, frame.render_passes.size());
  const QuadList& contrib_delegated_quad_list =
      frame.render_passes[0]->quad_list;
  ASSERT_EQ(2u, contrib_delegated_quad_list.size());
  const QuadList& root_delegated_quad_list = frame.render_passes[1]->quad_list;
  ASSERT_EQ(5u, root_delegated_quad_list.size());
  const SharedQuadState* root_delegated_shared_quad_state =
      root_delegated_quad_list[0]->shared_quad_state;
  const SharedQuadState* contrib_delegated_shared_quad_state =
      contrib_delegated_quad_list[0]->shared_quad_state;

  // When the layer owns a surface, the quads don't need to be clipped
  // further than they already specify. If they aren't clipped, then their
  // clip rect is ignored, and they are not set as clipped.
  EXPECT_FALSE(root_delegated_shared_quad_state->is_clipped);

  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  host_impl_->DidDrawAllLayers(frame);
}

TEST_F(DelegatedRendererLayerImplTestClip,
       QuadsClipped_LayerUnclipped_Surface) {
  root_delegated_render_pass_is_clipped_ = true;
  clip_delegated_renderer_layer_ = false;
  SetUpTest();

  delegated_renderer_layer_->SetForceRenderSurface(true);

  LayerTreeHostImpl::FrameData frame;
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));

  ASSERT_EQ(3u, frame.render_passes.size());
  const QuadList& contrib_delegated_quad_list =
      frame.render_passes[0]->quad_list;
  ASSERT_EQ(2u, contrib_delegated_quad_list.size());
  const QuadList& root_delegated_quad_list = frame.render_passes[1]->quad_list;
  ASSERT_EQ(5u, root_delegated_quad_list.size());
  const SharedQuadState* root_delegated_shared_quad_state =
      root_delegated_quad_list[0]->shared_quad_state;
  const SharedQuadState* contrib_delegated_shared_quad_state =
      contrib_delegated_quad_list[0]->shared_quad_state;

  // When the quads have a clip of their own, it is used.
  EXPECT_EQ(gfx::Rect(5, 5, 40, 40).ToString(),
            root_delegated_shared_quad_state->clip_rect.ToString());
  // Quads came with a clip rect.
  EXPECT_TRUE(root_delegated_shared_quad_state->is_clipped);

  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  host_impl_->DidDrawAllLayers(frame);
}

TEST_F(DelegatedRendererLayerImplTestClip,
       QuadsUnclipped_LayerClipped_Surface) {
  root_delegated_render_pass_is_clipped_ = false;
  clip_delegated_renderer_layer_ = true;
  SetUpTest();

  delegated_renderer_layer_->SetForceRenderSurface(true);

  LayerTreeHostImpl::FrameData frame;
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));

  ASSERT_EQ(3u, frame.render_passes.size());
  const QuadList& contrib_delegated_quad_list =
      frame.render_passes[0]->quad_list;
  ASSERT_EQ(2u, contrib_delegated_quad_list.size());
  const QuadList& root_delegated_quad_list = frame.render_passes[1]->quad_list;
  ASSERT_EQ(5u, root_delegated_quad_list.size());
  const SharedQuadState* root_delegated_shared_quad_state =
      root_delegated_quad_list[0]->shared_quad_state;
  const SharedQuadState* contrib_delegated_shared_quad_state =
      contrib_delegated_quad_list[0]->shared_quad_state;

  // When the layer owns a surface, the quads don't need to be clipped
  // further than they already specify. If they aren't clipped, then their
  // clip rect is ignored, and they are not set as clipped.
  EXPECT_FALSE(root_delegated_shared_quad_state->is_clipped);

  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  host_impl_->DidDrawAllLayers(frame);
}

TEST_F(DelegatedRendererLayerImplTestClip, QuadsClipped_LayerClipped_Surface) {
  root_delegated_render_pass_is_clipped_ = true;
  clip_delegated_renderer_layer_ = true;
  SetUpTest();

  delegated_renderer_layer_->SetForceRenderSurface(true);

  LayerTreeHostImpl::FrameData frame;
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));

  ASSERT_EQ(3u, frame.render_passes.size());
  const QuadList& contrib_delegated_quad_list =
      frame.render_passes[0]->quad_list;
  ASSERT_EQ(2u, contrib_delegated_quad_list.size());
  const QuadList& root_delegated_quad_list = frame.render_passes[1]->quad_list;
  ASSERT_EQ(5u, root_delegated_quad_list.size());
  const SharedQuadState* root_delegated_shared_quad_state =
      root_delegated_quad_list[0]->shared_quad_state;
  const SharedQuadState* contrib_delegated_shared_quad_state =
      contrib_delegated_quad_list[0]->shared_quad_state;

  // When the quads have a clip of their own, it is used, but it is
  // combined with the clip rect of the delegated renderer layer. If the
  // layer owns a surface, then it does not have a clip rect of its own.
  EXPECT_EQ(gfx::Rect(5, 5, 40, 40).ToString(),
            root_delegated_shared_quad_state->clip_rect.ToString());
  // Quads came with a clip rect.
  EXPECT_TRUE(root_delegated_shared_quad_state->is_clipped);

  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  host_impl_->DidDrawAllLayers(frame);
}

}  // namespace
}  // namespace cc
