// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/display.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/test/null_task_runner.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/texture_mailbox_deleter.h"
#include "cc/quads/render_pass.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/scheduler_test_common.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/quads/copy_output_result.h"
#include "components/viz/common/resources/shared_bitmap_manager.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/local_surface_id_allocator.h"
#include "components/viz/service/display/display_client.h"
#include "components/viz/service/display/display_scheduler.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::AnyNumber;

namespace viz {
namespace {

static constexpr FrameSinkId kArbitraryFrameSinkId(3, 3);
static constexpr FrameSinkId kAnotherFrameSinkId(4, 4);

class TestSoftwareOutputDevice : public cc::SoftwareOutputDevice {
 public:
  TestSoftwareOutputDevice() {}

  gfx::Rect damage_rect() const { return damage_rect_; }
  gfx::Size viewport_pixel_size() const { return viewport_pixel_size_; }
};

class TestDisplayScheduler : public DisplayScheduler {
 public:
  explicit TestDisplayScheduler(BeginFrameSource* begin_frame_source,
                                base::SingleThreadTaskRunner* task_runner)
      : DisplayScheduler(begin_frame_source, task_runner, 1),
        damaged(false),
        display_resized_(false),
        has_new_root_surface(false),
        swapped(false) {}

  ~TestDisplayScheduler() override {}

  void DisplayResized() override { display_resized_ = true; }

  void SetNewRootSurface(const SurfaceId& root_surface_id) override {
    has_new_root_surface = true;
  }

  void ProcessSurfaceDamage(const SurfaceId& surface_id,
                            const BeginFrameAck& ack,
                            bool display_damaged) override {
    if (display_damaged) {
      damaged = true;
      needs_draw_ = true;
    }
  }

  void DidSwapBuffers() override { swapped = true; }

  void ResetDamageForTest() {
    damaged = false;
    display_resized_ = false;
    has_new_root_surface = false;
  }

  bool damaged;
  bool display_resized_;
  bool has_new_root_surface;
  bool swapped;
};

class DisplayTest : public testing::Test {
 public:
  DisplayTest()
      : support_(
            CompositorFrameSinkSupport::Create(nullptr,
                                               &manager_,
                                               kArbitraryFrameSinkId,
                                               true /* is_root */,
                                               true /* needs_sync_points */)),
        task_runner_(new base::NullTaskRunner) {}

  ~DisplayTest() override { support_->EvictCurrentSurface(); }

  void SetUpDisplay(const RendererSettings& settings,
                    std::unique_ptr<cc::TestWebGraphicsContext3D> context) {
    begin_frame_source_.reset(new StubBeginFrameSource);

    std::unique_ptr<cc::FakeOutputSurface> output_surface;
    if (context) {
      auto provider = cc::TestContextProvider::Create(std::move(context));
      provider->BindToCurrentThread();
      output_surface = cc::FakeOutputSurface::Create3d(std::move(provider));
    } else {
      auto device = base::MakeUnique<TestSoftwareOutputDevice>();
      software_output_device_ = device.get();
      output_surface = cc::FakeOutputSurface::CreateSoftware(std::move(device));
    }
    output_surface_ = output_surface.get();
    auto scheduler = base::MakeUnique<TestDisplayScheduler>(
        begin_frame_source_.get(), task_runner_.get());
    scheduler_ = scheduler.get();
    display_ = CreateDisplay(settings, kArbitraryFrameSinkId,
                             std::move(scheduler), std::move(output_surface));
    manager_.RegisterBeginFrameSource(begin_frame_source_.get(),
                                      kArbitraryFrameSinkId);
  }

  std::unique_ptr<Display> CreateDisplay(
      const RendererSettings& settings,
      const FrameSinkId& frame_sink_id,
      std::unique_ptr<DisplayScheduler> scheduler,
      std::unique_ptr<cc::OutputSurface> output_surface) {
    auto display = base::MakeUnique<Display>(
        &shared_bitmap_manager_, nullptr /* gpu_memory_buffer_manager */,
        settings, frame_sink_id, std::move(output_surface),
        std::move(scheduler),
        base::MakeUnique<cc::TextureMailboxDeleter>(task_runner_.get()));
    display->SetVisible(true);
    return display;
  }

  void TearDownDisplay() {
    // Only call UnregisterBeginFrameSource if SetupDisplay has been called.
    if (begin_frame_source_)
      manager_.UnregisterBeginFrameSource(begin_frame_source_.get());
  }

 protected:
  void SubmitCompositorFrame(cc::RenderPassList* pass_list,
                             const LocalSurfaceId& local_surface_id) {
    cc::CompositorFrame frame = test::MakeCompositorFrame();
    pass_list->swap(frame.render_pass_list);

    support_->SubmitCompositorFrame(local_surface_id, std::move(frame));
  }

  FrameSinkManagerImpl manager_;
  std::unique_ptr<CompositorFrameSinkSupport> support_;
  LocalSurfaceIdAllocator id_allocator_;
  scoped_refptr<base::NullTaskRunner> task_runner_;
  cc::TestSharedBitmapManager shared_bitmap_manager_;
  std::unique_ptr<BeginFrameSource> begin_frame_source_;
  std::unique_ptr<Display> display_;
  TestSoftwareOutputDevice* software_output_device_ = nullptr;
  cc::FakeOutputSurface* output_surface_ = nullptr;
  TestDisplayScheduler* scheduler_ = nullptr;
};

class StubDisplayClient : public DisplayClient {
 public:
  void DisplayOutputSurfaceLost() override {}
  void DisplayWillDrawAndSwap(
      bool will_draw_and_swap,
      const cc::RenderPassList& render_passes) override {}
  void DisplayDidDrawAndSwap() override {}
};

void CopyCallback(bool* called, std::unique_ptr<CopyOutputResult> result) {
  *called = true;
}

// Check that frame is damaged and swapped only under correct conditions.
TEST_F(DisplayTest, DisplayDamaged) {
  RendererSettings settings;
  settings.partial_swap_enabled = true;
  settings.finish_rendering_on_resize = true;
  SetUpDisplay(settings, nullptr);
  gfx::ColorSpace color_space_1 = gfx::ColorSpace::CreateXYZD50();
  gfx::ColorSpace color_space_2 = gfx::ColorSpace::CreateSCRGBLinear();

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());
  display_->SetColorSpace(color_space_1, color_space_1);

  LocalSurfaceId local_surface_id(id_allocator_.GenerateId());
  EXPECT_FALSE(scheduler_->damaged);
  EXPECT_FALSE(scheduler_->has_new_root_surface);
  display_->SetLocalSurfaceId(local_surface_id, 1.f);
  EXPECT_FALSE(scheduler_->damaged);
  EXPECT_FALSE(scheduler_->display_resized_);
  EXPECT_TRUE(scheduler_->has_new_root_surface);

  scheduler_->ResetDamageForTest();
  display_->Resize(gfx::Size(100, 100));
  EXPECT_FALSE(scheduler_->damaged);
  EXPECT_TRUE(scheduler_->display_resized_);
  EXPECT_FALSE(scheduler_->has_new_root_surface);

  // First draw from surface should have full damage.
  cc::RenderPassList pass_list;
  auto pass = cc::RenderPass::Create();
  pass->output_rect = gfx::Rect(0, 0, 100, 100);
  pass->damage_rect = gfx::Rect(10, 10, 1, 1);
  pass->id = 1u;
  pass_list.push_back(std::move(pass));

  scheduler_->ResetDamageForTest();
  SubmitCompositorFrame(&pass_list, local_surface_id);
  EXPECT_TRUE(scheduler_->damaged);
  EXPECT_FALSE(scheduler_->display_resized_);
  EXPECT_FALSE(scheduler_->has_new_root_surface);

  EXPECT_FALSE(scheduler_->swapped);
  EXPECT_EQ(0u, output_surface_->num_sent_frames());
  EXPECT_EQ(gfx::ColorSpace(), output_surface_->last_reshape_color_space());
  display_->DrawAndSwap();
  EXPECT_EQ(color_space_1, output_surface_->last_reshape_color_space());
  EXPECT_TRUE(scheduler_->swapped);
  EXPECT_EQ(1u, output_surface_->num_sent_frames());
  EXPECT_EQ(gfx::Size(100, 100),
            software_output_device_->viewport_pixel_size());
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100), software_output_device_->damage_rect());
  {
    // Only damaged portion should be swapped.
    pass = cc::RenderPass::Create();
    pass->output_rect = gfx::Rect(0, 0, 100, 100);
    pass->damage_rect = gfx::Rect(10, 10, 1, 1);
    pass->id = 1u;

    pass_list.push_back(std::move(pass));
    scheduler_->ResetDamageForTest();
    SubmitCompositorFrame(&pass_list, local_surface_id);
    EXPECT_TRUE(scheduler_->damaged);
    EXPECT_FALSE(scheduler_->display_resized_);
    EXPECT_FALSE(scheduler_->has_new_root_surface);

    scheduler_->swapped = false;
    EXPECT_EQ(color_space_1, output_surface_->last_reshape_color_space());
    display_->SetColorSpace(color_space_2, color_space_2);
    display_->DrawAndSwap();
    EXPECT_EQ(color_space_2, output_surface_->last_reshape_color_space());
    EXPECT_TRUE(scheduler_->swapped);
    EXPECT_EQ(2u, output_surface_->num_sent_frames());
    EXPECT_EQ(gfx::Size(100, 100),
              software_output_device_->viewport_pixel_size());
    EXPECT_EQ(gfx::Rect(10, 10, 1, 1), software_output_device_->damage_rect());
  }

  {
    // Pass has no damage so shouldn't be swapped.
    pass = cc::RenderPass::Create();
    pass->output_rect = gfx::Rect(0, 0, 100, 100);
    pass->damage_rect = gfx::Rect(10, 10, 0, 0);
    pass->id = 1u;

    pass_list.push_back(std::move(pass));
    scheduler_->ResetDamageForTest();
    SubmitCompositorFrame(&pass_list, local_surface_id);
    EXPECT_TRUE(scheduler_->damaged);
    EXPECT_FALSE(scheduler_->display_resized_);
    EXPECT_FALSE(scheduler_->has_new_root_surface);

    scheduler_->swapped = false;
    display_->DrawAndSwap();
    EXPECT_TRUE(scheduler_->swapped);
    EXPECT_EQ(2u, output_surface_->num_sent_frames());
  }

  {
    // Pass is wrong size so shouldn't be swapped.
    pass = cc::RenderPass::Create();
    pass->output_rect = gfx::Rect(0, 0, 99, 99);
    pass->damage_rect = gfx::Rect(10, 10, 10, 10);
    pass->id = 1u;

    local_surface_id = id_allocator_.GenerateId();
    display_->SetLocalSurfaceId(local_surface_id, 1.f);

    pass_list.push_back(std::move(pass));
    scheduler_->ResetDamageForTest();
    SubmitCompositorFrame(&pass_list, local_surface_id);
    EXPECT_TRUE(scheduler_->damaged);
    EXPECT_FALSE(scheduler_->display_resized_);
    EXPECT_FALSE(scheduler_->has_new_root_surface);

    scheduler_->swapped = false;
    display_->DrawAndSwap();
    EXPECT_TRUE(scheduler_->swapped);
    EXPECT_EQ(2u, output_surface_->num_sent_frames());
  }

  {
    // Previous frame wasn't swapped, so next swap should have full damage.
    pass = cc::RenderPass::Create();
    pass->output_rect = gfx::Rect(0, 0, 100, 100);
    pass->damage_rect = gfx::Rect(10, 10, 0, 0);
    pass->id = 1u;

    local_surface_id = id_allocator_.GenerateId();
    display_->SetLocalSurfaceId(local_surface_id, 1.f);

    pass_list.push_back(std::move(pass));
    scheduler_->ResetDamageForTest();
    SubmitCompositorFrame(&pass_list, local_surface_id);
    EXPECT_TRUE(scheduler_->damaged);
    EXPECT_FALSE(scheduler_->display_resized_);
    EXPECT_FALSE(scheduler_->has_new_root_surface);

    scheduler_->swapped = false;
    display_->DrawAndSwap();
    EXPECT_TRUE(scheduler_->swapped);
    EXPECT_EQ(3u, output_surface_->num_sent_frames());
    EXPECT_EQ(gfx::Rect(0, 0, 100, 100),
              software_output_device_->damage_rect());
  }

  {
    // Pass has copy output request so should be swapped.
    pass = cc::RenderPass::Create();
    pass->output_rect = gfx::Rect(0, 0, 100, 100);
    pass->damage_rect = gfx::Rect(10, 10, 0, 0);
    bool copy_called = false;
    pass->copy_requests.push_back(CopyOutputRequest::CreateRequest(
        base::BindOnce(&CopyCallback, &copy_called)));
    pass->id = 1u;

    pass_list.push_back(std::move(pass));
    scheduler_->ResetDamageForTest();
    SubmitCompositorFrame(&pass_list, local_surface_id);
    EXPECT_TRUE(scheduler_->damaged);
    EXPECT_FALSE(scheduler_->display_resized_);
    EXPECT_FALSE(scheduler_->has_new_root_surface);

    scheduler_->swapped = false;
    display_->DrawAndSwap();
    EXPECT_TRUE(scheduler_->swapped);
    EXPECT_EQ(4u, output_surface_->num_sent_frames());
    EXPECT_TRUE(copy_called);
  }

  // Pass has no damage, so shouldn't be swapped, but latency info should be
  // saved for next swap.
  {
    pass = cc::RenderPass::Create();
    pass->output_rect = gfx::Rect(0, 0, 100, 100);
    pass->damage_rect = gfx::Rect(10, 10, 0, 0);
    pass->id = 1u;

    pass_list.push_back(std::move(pass));
    scheduler_->ResetDamageForTest();

    cc::CompositorFrame frame = test::MakeCompositorFrame();
    pass_list.swap(frame.render_pass_list);
    frame.metadata.latency_info.push_back(ui::LatencyInfo());

    support_->SubmitCompositorFrame(local_surface_id, std::move(frame));
    EXPECT_TRUE(scheduler_->damaged);
    EXPECT_FALSE(scheduler_->display_resized_);
    EXPECT_FALSE(scheduler_->has_new_root_surface);

    scheduler_->swapped = false;
    display_->DrawAndSwap();
    EXPECT_TRUE(scheduler_->swapped);
    EXPECT_EQ(4u, output_surface_->num_sent_frames());
  }

  // Resize should cause a swap if no frame was swapped at the previous size.
  {
    local_surface_id = id_allocator_.GenerateId();
    display_->SetLocalSurfaceId(local_surface_id, 1.f);
    scheduler_->swapped = false;
    display_->Resize(gfx::Size(200, 200));
    EXPECT_FALSE(scheduler_->swapped);
    EXPECT_EQ(4u, output_surface_->num_sent_frames());

    pass = cc::RenderPass::Create();
    pass->output_rect = gfx::Rect(0, 0, 200, 200);
    pass->damage_rect = gfx::Rect(10, 10, 10, 10);
    pass->id = 1u;

    pass_list.push_back(std::move(pass));
    scheduler_->ResetDamageForTest();

    cc::CompositorFrame frame = test::MakeCompositorFrame();
    pass_list.swap(frame.render_pass_list);

    support_->SubmitCompositorFrame(local_surface_id, std::move(frame));
    EXPECT_TRUE(scheduler_->damaged);
    EXPECT_FALSE(scheduler_->display_resized_);
    EXPECT_FALSE(scheduler_->has_new_root_surface);

    scheduler_->swapped = false;
    display_->Resize(gfx::Size(100, 100));
    EXPECT_TRUE(scheduler_->swapped);
    EXPECT_EQ(5u, output_surface_->num_sent_frames());

    // Latency info from previous frame should be sent now.
    EXPECT_EQ(1u, output_surface_->last_sent_frame()->latency_info.size());
  }

  {
    local_surface_id = id_allocator_.GenerateId();
    display_->SetLocalSurfaceId(local_surface_id, 1.0f);
    // Surface that's damaged completely should be resized and swapped.
    pass = cc::RenderPass::Create();
    pass->output_rect = gfx::Rect(0, 0, 99, 99);
    pass->damage_rect = gfx::Rect(0, 0, 99, 99);
    pass->id = 1u;

    pass_list.push_back(std::move(pass));
    scheduler_->ResetDamageForTest();
    SubmitCompositorFrame(&pass_list, local_surface_id);
    EXPECT_TRUE(scheduler_->damaged);
    EXPECT_FALSE(scheduler_->display_resized_);
    EXPECT_FALSE(scheduler_->has_new_root_surface);

    scheduler_->swapped = false;
    display_->DrawAndSwap();
    EXPECT_TRUE(scheduler_->swapped);
    EXPECT_EQ(6u, output_surface_->num_sent_frames());
    EXPECT_EQ(gfx::Size(100, 100),
              software_output_device_->viewport_pixel_size());
    EXPECT_EQ(gfx::Rect(0, 0, 100, 100),
              software_output_device_->damage_rect());
    EXPECT_EQ(0u, output_surface_->last_sent_frame()->latency_info.size());
  }
  TearDownDisplay();
}

// Check LatencyInfo storage is cleaned up if it exceeds the limit.
TEST_F(DisplayTest, MaxLatencyInfoCap) {
  RendererSettings settings;
  settings.partial_swap_enabled = true;
  settings.finish_rendering_on_resize = true;
  SetUpDisplay(settings, nullptr);
  gfx::ColorSpace color_space_1 = gfx::ColorSpace::CreateXYZD50();
  gfx::ColorSpace color_space_2 = gfx::ColorSpace::CreateSCRGBLinear();

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());
  display_->SetColorSpace(color_space_1, color_space_1);

  LocalSurfaceId local_surface_id(id_allocator_.GenerateId());
  display_->SetLocalSurfaceId(local_surface_id, 1.f);

  scheduler_->ResetDamageForTest();
  display_->Resize(gfx::Size(100, 100));

  cc::RenderPassList pass_list;
  auto pass = cc::RenderPass::Create();
  pass->output_rect = gfx::Rect(0, 0, 100, 100);
  pass->damage_rect = gfx::Rect(10, 10, 1, 1);
  pass->id = 1u;
  pass_list.push_back(std::move(pass));

  scheduler_->ResetDamageForTest();
  SubmitCompositorFrame(&pass_list, local_surface_id);

  display_->DrawAndSwap();

  // This is the same as LatencyInfo::kMaxLatencyInfoNumber.
  const size_t max_latency_info_count = 100;
  for (size_t i = 0; i <= max_latency_info_count; ++i) {
    pass = cc::RenderPass::Create();
    pass->output_rect = gfx::Rect(0, 0, 100, 100);
    pass->damage_rect = gfx::Rect(10, 10, 0, 0);
    pass->id = 1u;

    pass_list.push_back(std::move(pass));
    scheduler_->ResetDamageForTest();

    cc::CompositorFrame frame = test::MakeCompositorFrame();
    pass_list.swap(frame.render_pass_list);
    frame.metadata.latency_info.push_back(ui::LatencyInfo());

    support_->SubmitCompositorFrame(local_surface_id, std::move(frame));

    display_->DrawAndSwap();

    if (i < max_latency_info_count)
      EXPECT_EQ(i + 1, display_->stored_latency_info_size_for_testing());
    else
      EXPECT_EQ(0u, display_->stored_latency_info_size_for_testing());
  }

  TearDownDisplay();
}

class MockedContext : public cc::TestWebGraphicsContext3D {
 public:
  MOCK_METHOD0(shallowFinishCHROMIUM, void());
};

TEST_F(DisplayTest, Finish) {
  LocalSurfaceId local_surface_id1(id_allocator_.GenerateId());
  LocalSurfaceId local_surface_id2(id_allocator_.GenerateId());

  RendererSettings settings;
  settings.partial_swap_enabled = true;
  settings.finish_rendering_on_resize = true;

  auto context = base::MakeUnique<MockedContext>();
  MockedContext* context_ptr = context.get();
  EXPECT_CALL(*context_ptr, shallowFinishCHROMIUM()).Times(0);

  SetUpDisplay(settings, std::move(context));

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());

  display_->SetLocalSurfaceId(local_surface_id1, 1.f);

  display_->Resize(gfx::Size(100, 100));

  {
    cc::RenderPassList pass_list;
    auto pass = cc::RenderPass::Create();
    pass->output_rect = gfx::Rect(0, 0, 100, 100);
    pass->damage_rect = gfx::Rect(10, 10, 1, 1);
    pass->id = 1u;
    pass_list.push_back(std::move(pass));

    SubmitCompositorFrame(&pass_list, local_surface_id1);
  }

  display_->DrawAndSwap();

  // First resize and draw shouldn't finish.
  testing::Mock::VerifyAndClearExpectations(context_ptr);

  EXPECT_CALL(*context_ptr, shallowFinishCHROMIUM());
  display_->Resize(gfx::Size(150, 150));
  testing::Mock::VerifyAndClearExpectations(context_ptr);

  // Another resize without a swap doesn't need to finish.
  EXPECT_CALL(*context_ptr, shallowFinishCHROMIUM()).Times(0);
  display_->SetLocalSurfaceId(local_surface_id2, 1.f);
  display_->Resize(gfx::Size(200, 200));
  testing::Mock::VerifyAndClearExpectations(context_ptr);

  EXPECT_CALL(*context_ptr, shallowFinishCHROMIUM()).Times(0);
  {
    cc::RenderPassList pass_list;
    auto pass = cc::RenderPass::Create();
    pass->output_rect = gfx::Rect(0, 0, 200, 200);
    pass->damage_rect = gfx::Rect(10, 10, 1, 1);
    pass->id = 1u;
    pass_list.push_back(std::move(pass));

    SubmitCompositorFrame(&pass_list, local_surface_id2);
  }

  display_->DrawAndSwap();

  testing::Mock::VerifyAndClearExpectations(context_ptr);

  EXPECT_CALL(*context_ptr, shallowFinishCHROMIUM());
  display_->Resize(gfx::Size(250, 250));
  testing::Mock::VerifyAndClearExpectations(context_ptr);
  TearDownDisplay();
}

class CountLossDisplayClient : public StubDisplayClient {
 public:
  CountLossDisplayClient() = default;

  void DisplayOutputSurfaceLost() override { ++loss_count_; }

  int loss_count() const { return loss_count_; }

 private:
  int loss_count_ = 0;
};

TEST_F(DisplayTest, ContextLossInformsClient) {
  SetUpDisplay(RendererSettings(), cc::TestWebGraphicsContext3D::Create());

  CountLossDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());

  // Verify DidLoseOutputSurface callback is hooked up correctly.
  EXPECT_EQ(0, client.loss_count());
  output_surface_->context_provider()->ContextGL()->LoseContextCHROMIUM(
      GL_GUILTY_CONTEXT_RESET_ARB, GL_INNOCENT_CONTEXT_RESET_ARB);
  output_surface_->context_provider()->ContextGL()->Flush();
  EXPECT_EQ(1, client.loss_count());
  TearDownDisplay();
}

// Regression test for https://crbug.com/727162: Submitting a CompositorFrame to
// a surface should only cause damage on the Display the surface belongs to.
// There should not be a side-effect on other Displays.
TEST_F(DisplayTest, CompositorFrameDamagesCorrectDisplay) {
  RendererSettings settings;
  LocalSurfaceId local_surface_id(id_allocator_.GenerateId());

  // Set up first display.
  SetUpDisplay(settings, nullptr);
  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());
  display_->SetLocalSurfaceId(local_surface_id, 1.f);

  // Set up second frame sink + display.
  auto support2 = CompositorFrameSinkSupport::Create(
      nullptr, &manager_, kAnotherFrameSinkId, true /* is_root */,
      true /* needs_sync_points */);
  auto begin_frame_source2 = base::MakeUnique<StubBeginFrameSource>();
  auto scheduler_for_display2 = base::MakeUnique<TestDisplayScheduler>(
      begin_frame_source2.get(), task_runner_.get());
  TestDisplayScheduler* scheduler2 = scheduler_for_display2.get();
  auto display2 = CreateDisplay(
      settings, kAnotherFrameSinkId, std::move(scheduler_for_display2),
      cc::FakeOutputSurface::CreateSoftware(
          base::MakeUnique<TestSoftwareOutputDevice>()));
  manager_.RegisterBeginFrameSource(begin_frame_source2.get(),
                                    kAnotherFrameSinkId);
  StubDisplayClient client2;
  display2->Initialize(&client2, manager_.surface_manager());
  display2->SetLocalSurfaceId(local_surface_id, 1.f);

  display_->Resize(gfx::Size(100, 100));
  display2->Resize(gfx::Size(100, 100));

  scheduler_->ResetDamageForTest();
  scheduler2->ResetDamageForTest();
  EXPECT_FALSE(scheduler_->damaged);
  EXPECT_FALSE(scheduler2->damaged);

  // Submit a frame for display_ with full damage.
  cc::RenderPassList pass_list;
  auto pass = cc::RenderPass::Create();
  pass->output_rect = gfx::Rect(0, 0, 100, 100);
  pass->damage_rect = gfx::Rect(10, 10, 1, 1);
  pass->id = 1;
  pass_list.push_back(std::move(pass));

  SubmitCompositorFrame(&pass_list, local_surface_id);

  // Should have damaged only display_ but not display2.
  EXPECT_TRUE(scheduler_->damaged);
  EXPECT_FALSE(scheduler2->damaged);
  manager_.UnregisterBeginFrameSource(begin_frame_source2.get());
  TearDownDisplay();
}

}  // namespace
}  // namespace viz
