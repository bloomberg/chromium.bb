// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/delegating_renderer.h"

#include <stdint.h>

#include "cc/test/fake_output_surface.h"
#include "cc/test/layer_tree_test.h"
#include "cc/test/render_pass_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

class DelegatingRendererTest : public LayerTreeTest {};

class DelegatingRendererTestDraw : public DelegatingRendererTest {
 public:
  void BeginTest() override {
    layer_tree()->SetPageScaleFactorAndLimits(1.f, 0.5f, 4.f);
    PostSetNeedsCommitToMainThread();
  }

  void AfterTest() override {}

  void DrawLayersOnThread(LayerTreeHostImpl* host_impl) override {
    EXPECT_EQ(0, num_swaps_);
    drawn_viewport_ = host_impl->DeviceViewport();
  }

  void DisplayReceivedCompositorFrameOnThread(
      const CompositorFrame& frame) override {
    EXPECT_EQ(1, ++num_swaps_);

    DelegatedFrameData* last_frame_data = frame.delegated_frame_data.get();
    ASSERT_TRUE(frame.delegated_frame_data);
    EXPECT_FALSE(frame.gl_frame_data);
    EXPECT_EQ(drawn_viewport_,
              last_frame_data->render_pass_list.back()->output_rect);
    EXPECT_EQ(0.5f, frame.metadata.min_page_scale_factor);
    EXPECT_EQ(4.f, frame.metadata.max_page_scale_factor);

    EXPECT_EQ(0u, frame.delegated_frame_data->resource_list.size());
    EXPECT_EQ(1u, frame.delegated_frame_data->render_pass_list.size());

    EndTest();
  }

  int num_swaps_ = 0;
  gfx::Rect drawn_viewport_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(DelegatingRendererTestDraw);

class DelegatingRendererTestResources : public DelegatingRendererTest {
 public:
  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void AfterTest() override {}

  DrawResult PrepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                   LayerTreeHostImpl::FrameData* frame,
                                   DrawResult draw_result) override {
    frame->render_passes.clear();

    RenderPass* child_pass =
        AddRenderPass(&frame->render_passes, RenderPassId(2, 1),
                      gfx::Rect(3, 3, 10, 10), gfx::Transform());
    gpu::SyncToken mailbox_sync_token;
    AddOneOfEveryQuadType(child_pass, host_impl->resource_provider(),
                          RenderPassId(0, 0), &mailbox_sync_token);

    RenderPass* pass = AddRenderPass(&frame->render_passes, RenderPassId(1, 1),
                                     gfx::Rect(3, 3, 10, 10), gfx::Transform());
    AddOneOfEveryQuadType(pass, host_impl->resource_provider(), child_pass->id,
                          &mailbox_sync_token);
    return draw_result;
  }

  void DisplayReceivedCompositorFrameOnThread(
      const CompositorFrame& frame) override {
    ASSERT_TRUE(frame.delegated_frame_data);

    EXPECT_EQ(2u, frame.delegated_frame_data->render_pass_list.size());
    // Each render pass has 10 resources in it. And the root render pass has a
    // mask resource used when drawing the child render pass, as well as its
    // replica (it's added twice). The number 10 may change if
    // AppendOneOfEveryQuadType() is updated, and the value here should be
    // updated accordingly.
    EXPECT_EQ(22u, frame.delegated_frame_data->resource_list.size());

    EndTest();
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(DelegatingRendererTestResources);

}  // namespace cc
