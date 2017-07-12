// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/direct_layer_tree_frame_sink.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "cc/output/renderer_settings.h"
#include "cc/output/texture_mailbox_deleter.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/scheduler/delay_based_time_source.h"
#include "cc/surfaces/frame_sink_manager.h"
#include "cc/test/begin_frame_args_test.h"
#include "cc/test/compositor_frame_helpers.h"
#include "cc/test/fake_layer_tree_frame_sink_client.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/ordered_simple_task_runner.h"
#include "cc/test/test_context_provider.h"
#include "cc/test/test_gpu_memory_buffer_manager.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "components/viz/common/frame_sink_id.h"
#include "components/viz/common/local_surface_id_allocator.h"
#include "components/viz/service/display/display.h"
#include "components/viz/service/display/display_scheduler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
namespace {

static constexpr FrameSinkId kArbitraryFrameSinkId(1, 1);

class TestDirectLayerTreeFrameSink : public DirectLayerTreeFrameSink {
 public:
  using DirectLayerTreeFrameSink::DirectLayerTreeFrameSink;

  CompositorFrameSinkSupport* support() const { return support_.get(); }
};

class DirectLayerTreeFrameSinkTest : public testing::Test {
 public:
  DirectLayerTreeFrameSinkTest()
      : now_src_(new base::SimpleTestTickClock()),
        task_runner_(new cc::OrderedSimpleTaskRunner(now_src_.get(), true)),
        display_size_(1920, 1080),
        display_rect_(display_size_),
        context_provider_(cc::TestContextProvider::Create()) {
    frame_sink_manager_.RegisterFrameSinkId(kArbitraryFrameSinkId);

    auto display_output_surface = cc::FakeOutputSurface::Create3d();
    display_output_surface_ = display_output_surface.get();

    begin_frame_source_ = base::MakeUnique<cc::BackToBackBeginFrameSource>(
        base::MakeUnique<cc::DelayBasedTimeSource>(task_runner_.get()));

    int max_frames_pending = 2;
    std::unique_ptr<DisplayScheduler> scheduler(new DisplayScheduler(
        begin_frame_source_.get(), task_runner_.get(), max_frames_pending));

    display_.reset(new Display(
        &bitmap_manager_, &gpu_memory_buffer_manager_, cc::RendererSettings(),
        kArbitraryFrameSinkId, std::move(display_output_surface),
        std::move(scheduler),
        base::MakeUnique<cc::TextureMailboxDeleter>(task_runner_.get())));
    layer_tree_frame_sink_ = base::MakeUnique<TestDirectLayerTreeFrameSink>(
        kArbitraryFrameSinkId, &frame_sink_manager_, display_.get(),
        context_provider_, nullptr, &gpu_memory_buffer_manager_,
        &bitmap_manager_);

    layer_tree_frame_sink_->BindToClient(&layer_tree_frame_sink_client_);
    display_->Resize(display_size_);
    display_->SetVisible(true);

    EXPECT_FALSE(
        layer_tree_frame_sink_client_.did_lose_layer_tree_frame_sink_called());
  }

  ~DirectLayerTreeFrameSinkTest() override {
    layer_tree_frame_sink_->DetachFromClient();
  }

  void SwapBuffersWithDamage(const gfx::Rect& damage_rect) {
    auto render_pass = cc::RenderPass::Create();
    render_pass->SetNew(1, display_rect_, damage_rect, gfx::Transform());

    cc::CompositorFrame frame = cc::test::MakeEmptyCompositorFrame();
    frame.metadata.begin_frame_ack = cc::BeginFrameAck(0, 1, 1, true);
    frame.render_pass_list.push_back(std::move(render_pass));

    layer_tree_frame_sink_->SubmitCompositorFrame(std::move(frame));
  }

  void SetUp() override {
    // Draw the first frame to start in an "unlocked" state.
    SwapBuffersWithDamage(display_rect_);

    EXPECT_EQ(0u, display_output_surface_->num_sent_frames());
    task_runner_->RunUntilIdle();
    EXPECT_EQ(1u, display_output_surface_->num_sent_frames());
  }

 protected:
  std::unique_ptr<base::SimpleTestTickClock> now_src_;
  scoped_refptr<cc::OrderedSimpleTaskRunner> task_runner_;

  const gfx::Size display_size_;
  const gfx::Rect display_rect_;
  cc::FrameSinkManager frame_sink_manager_;
  cc::TestSharedBitmapManager bitmap_manager_;
  cc::TestGpuMemoryBufferManager gpu_memory_buffer_manager_;

  scoped_refptr<cc::TestContextProvider> context_provider_;
  cc::FakeOutputSurface* display_output_surface_ = nullptr;
  std::unique_ptr<cc::BackToBackBeginFrameSource> begin_frame_source_;
  std::unique_ptr<Display> display_;
  cc::FakeLayerTreeFrameSinkClient layer_tree_frame_sink_client_;
  std::unique_ptr<TestDirectLayerTreeFrameSink> layer_tree_frame_sink_;
};

TEST_F(DirectLayerTreeFrameSinkTest, DamageTriggersSwapBuffers) {
  SwapBuffersWithDamage(display_rect_);
  EXPECT_EQ(1u, display_output_surface_->num_sent_frames());
  task_runner_->RunUntilIdle();
  EXPECT_EQ(2u, display_output_surface_->num_sent_frames());
}

TEST_F(DirectLayerTreeFrameSinkTest, NoDamageDoesNotTriggerSwapBuffers) {
  SwapBuffersWithDamage(gfx::Rect());
  EXPECT_EQ(1u, display_output_surface_->num_sent_frames());
  task_runner_->RunUntilIdle();
  EXPECT_EQ(1u, display_output_surface_->num_sent_frames());
}

TEST_F(DirectLayerTreeFrameSinkTest, SuspendedDoesNotTriggerSwapBuffers) {
  SwapBuffersWithDamage(display_rect_);
  EXPECT_EQ(1u, display_output_surface_->num_sent_frames());
  display_output_surface_->set_suspended_for_recycle(true);
  task_runner_->RunUntilIdle();
  EXPECT_EQ(1u, display_output_surface_->num_sent_frames());
  SwapBuffersWithDamage(display_rect_);
  task_runner_->RunUntilIdle();
  EXPECT_EQ(1u, display_output_surface_->num_sent_frames());
  display_output_surface_->set_suspended_for_recycle(false);
  SwapBuffersWithDamage(display_rect_);
  task_runner_->RunUntilIdle();
  EXPECT_EQ(2u, display_output_surface_->num_sent_frames());
}

}  // namespace
}  // namespace viz
