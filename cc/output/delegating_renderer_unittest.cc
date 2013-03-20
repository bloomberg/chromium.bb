// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/delegating_renderer.h"

#include "cc/test/fake_output_surface.h"
#include "cc/test/layer_tree_test.h"
#include "cc/test/render_pass_test_common.h"
#include "cc/test/render_pass_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

class DelegatingRendererTest : public LayerTreeTest {
 public:
  DelegatingRendererTest() : LayerTreeTest(), output_surface_(NULL) {}
  virtual ~DelegatingRendererTest() {}

  virtual scoped_ptr<OutputSurface> CreateOutputSurface() OVERRIDE {
    scoped_ptr<TestWebGraphicsContext3D> context3d =
        TestWebGraphicsContext3D::Create(
            WebKit::WebGraphicsContext3D::Attributes());
    context3d_ = context3d.get();
    scoped_ptr<FakeOutputSurface> output_surface =
        FakeOutputSurface::CreateDelegating3d(
            context3d.PassAs<WebKit::WebGraphicsContext3D>());
    output_surface_ = output_surface.get();
    return output_surface.PassAs<OutputSurface>();
  }

 protected:
  TestWebGraphicsContext3D* context3d_;
  FakeOutputSurface* output_surface_;
};

class DelegatingRendererTestDraw : public DelegatingRendererTest {
 public:
  virtual void BeginTest() OVERRIDE {
    layer_tree_host()->SetPageScaleFactorAndLimits(1.f, 0.5f, 4.f);
    PostSetNeedsCommitToMainThread();
  }

  virtual void AfterTest() OVERRIDE {}

  virtual bool PrepareToDrawOnThread(
      LayerTreeHostImpl*, LayerTreeHostImpl::FrameData* frame, bool result)
      OVERRIDE {
    EXPECT_EQ(0u, output_surface_->num_sent_frames());

    CompositorFrame& last_frame = output_surface_->last_sent_frame();
    EXPECT_FALSE(last_frame.delegated_frame_data);
    EXPECT_FALSE(last_frame.gl_frame_data);
    EXPECT_EQ(0.f, last_frame.metadata.min_page_scale_factor);
    EXPECT_EQ(0.f, last_frame.metadata.max_page_scale_factor);
    return true;
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    EXPECT_EQ(1u, output_surface_->num_sent_frames());

    CompositorFrame& last_frame = output_surface_->last_sent_frame();
    DelegatedFrameData* last_frame_data = last_frame.delegated_frame_data.get();
    ASSERT_TRUE(last_frame.delegated_frame_data);
    EXPECT_FALSE(last_frame.gl_frame_data);
    EXPECT_EQ(
        gfx::Rect(host_impl->device_viewport_size()).ToString(),
        last_frame_data->render_pass_list.back()->output_rect.ToString());
    EXPECT_EQ(0.5f, last_frame.metadata.min_page_scale_factor);
    EXPECT_EQ(4.f, last_frame.metadata.max_page_scale_factor);

    EXPECT_EQ(
        0u, last_frame.delegated_frame_data->resource_list.size());
    EXPECT_EQ(1u, last_frame.delegated_frame_data->render_pass_list.size());

    EndTest();
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(DelegatingRendererTestDraw)

class DelegatingRendererTestResources : public DelegatingRendererTest {
 public:
  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
  }

  virtual void AfterTest() OVERRIDE {}

  virtual bool PrepareToDrawOnThread(
      LayerTreeHostImpl* host_impl,
      LayerTreeHostImpl::FrameData* frame,
      bool result) OVERRIDE {

    frame->render_passes.clear();
    frame->render_passes_by_id.clear();

    TestRenderPass* child_pass = addRenderPass(
        frame->render_passes,
        RenderPass::Id(2, 1),
        gfx::Rect(3, 3, 10, 10),
        gfx::Transform());
    child_pass->AppendOneOfEveryQuadType(
        host_impl->resource_provider(), RenderPass::Id(0, 0));

    TestRenderPass* pass = addRenderPass(
        frame->render_passes,
        RenderPass::Id(1, 1),
        gfx::Rect(3, 3, 10, 10),
        gfx::Transform());
    pass->AppendOneOfEveryQuadType(
        host_impl->resource_provider(), child_pass->id);
    return true;
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    EXPECT_EQ(1u, output_surface_->num_sent_frames());

    CompositorFrame& last_frame = output_surface_->last_sent_frame();
    ASSERT_TRUE(last_frame.delegated_frame_data);

    EXPECT_EQ(2u, last_frame.delegated_frame_data->render_pass_list.size());
    // Each render pass has 7 resources in it. And the root render pass has a
    // mask resource used when drawing the child render pass. The number 7 may
    // change if AppendOneOfEveryQuadType is updated, and the value here should
    // be updated accordingly.
    EXPECT_EQ(
        15u, last_frame.delegated_frame_data->resource_list.size());

    EndTest();
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(DelegatingRendererTestResources)

}  // namespace cc
