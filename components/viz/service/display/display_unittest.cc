// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/display.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/test/null_task_runner.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/scheduler_test_common.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/render_pass.h"
#include "components/viz/common/quads/render_pass_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/resources/shared_bitmap_manager.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/local_surface_id_allocator.h"
#include "components/viz/service/display/display_client.h"
#include "components/viz/service/display/display_scheduler.h"
#include "components/viz/service/display/texture_mailbox_deleter.h"
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

class TestSoftwareOutputDevice : public SoftwareOutputDevice {
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
      std::unique_ptr<OutputSurface> output_surface) {
    auto display = base::MakeUnique<Display>(
        &shared_bitmap_manager_, nullptr /* gpu_memory_buffer_manager */,
        settings, frame_sink_id, std::move(output_surface),
        std::move(scheduler),
        base::MakeUnique<TextureMailboxDeleter>(task_runner_.get()));
    display->SetVisible(true);
    return display;
  }

  void TearDownDisplay() {
    // Only call UnregisterBeginFrameSource if SetupDisplay has been called.
    if (begin_frame_source_)
      manager_.UnregisterBeginFrameSource(begin_frame_source_.get());
  }

 protected:
  void SubmitCompositorFrame(RenderPassList* pass_list,
                             const LocalSurfaceId& local_surface_id) {
    CompositorFrame frame = test::MakeCompositorFrame();
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
  void DisplayWillDrawAndSwap(bool will_draw_and_swap,
                              const RenderPassList& render_passes) override {}
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
  RenderPassList pass_list;
  auto pass = RenderPass::Create();
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
    pass = RenderPass::Create();
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
    pass = RenderPass::Create();
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
    pass = RenderPass::Create();
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
    pass = RenderPass::Create();
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
    pass = RenderPass::Create();
    pass->output_rect = gfx::Rect(0, 0, 100, 100);
    pass->damage_rect = gfx::Rect(10, 10, 0, 0);
    bool copy_called = false;
    pass->copy_requests.push_back(std::make_unique<CopyOutputRequest>(
        CopyOutputRequest::ResultFormat::RGBA_BITMAP,
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
    pass = RenderPass::Create();
    pass->output_rect = gfx::Rect(0, 0, 100, 100);
    pass->damage_rect = gfx::Rect(10, 10, 0, 0);
    pass->id = 1u;

    pass_list.push_back(std::move(pass));
    scheduler_->ResetDamageForTest();

    CompositorFrame frame = test::MakeCompositorFrame();
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

    pass = RenderPass::Create();
    pass->output_rect = gfx::Rect(0, 0, 200, 200);
    pass->damage_rect = gfx::Rect(10, 10, 10, 10);
    pass->id = 1u;

    pass_list.push_back(std::move(pass));
    scheduler_->ResetDamageForTest();

    CompositorFrame frame = test::MakeCompositorFrame();
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
    pass = RenderPass::Create();
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

  RenderPassList pass_list;
  auto pass = RenderPass::Create();
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
    pass = RenderPass::Create();
    pass->output_rect = gfx::Rect(0, 0, 100, 100);
    pass->damage_rect = gfx::Rect(10, 10, 0, 0);
    pass->id = 1u;

    pass_list.push_back(std::move(pass));
    scheduler_->ResetDamageForTest();

    CompositorFrame frame = test::MakeCompositorFrame();
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
    RenderPassList pass_list;
    auto pass = RenderPass::Create();
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
    RenderPassList pass_list;
    auto pass = RenderPass::Create();
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
  RenderPassList pass_list;
  auto pass = RenderPass::Create();
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

// Check if draw occlusion does not remove any draw quads when no quads is being
// covered completely.
TEST_F(DisplayTest, DrawOcclusionWithNonCoveringDrawQuad) {
  SetUpDisplay(RendererSettings(), cc::TestWebGraphicsContext3D::Create());

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());

  CompositorFrame frame = test::MakeCompositorFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(50, 50, 100, 100);
  gfx::Rect rect3(25, 25, 50, 100);
  gfx::Rect rect4(150, 0, 50, 50);
  gfx::Rect rect5(0, 0, 120, 120);

  bool is_clipped = false;
  bool are_contents_opaque = true;
  float opacity = 1.f;
  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.front()
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();

  // +----+
  // |    |
  // +----+
  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, rect1, is_clipped,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);

    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // This is a base case, the quad list of compositor frame contains only one
    // quad, so quad_list still has size 1 after removing overdraw.
    EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->rect.ToString());
  }
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad2 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();

  // +----+
  // | +--|-+
  // +----+ |
  //   +----+
  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, rect1, is_clipped,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);

    shared_quad_state2->SetAll(gfx::Transform(), rect2, rect2, rect2,
                               is_clipped, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 0);

    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SK_ColorBLACK, false);

    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // Since |quad1| and |quad2| are partially overlapped, no quad can be
    // removed.
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->rect.ToString());
    EXPECT_EQ(rect2.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(1)
                                    ->rect.ToString());
  }

  // +----+
  // |+--+|
  // +----+
  //  +--+
  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, rect1, is_clipped,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);

    shared_quad_state2->SetAll(gfx::Transform(), rect3, rect3, rect3,
                               is_clipped, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 0);

    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect3, rect3, SK_ColorBLACK, false);

    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // Since |quad1| and |quad2| are partially overlapped, no quad can be
    // removed.
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->rect.ToString());
    EXPECT_EQ(rect3.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(1)
                                    ->rect.ToString());
  }

  // +----+   +--+
  // |    |   +--+
  // +----+
  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, rect1, is_clipped,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);

    shared_quad_state2->SetAll(gfx::Transform(), rect4, rect4, rect4,
                               is_clipped, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 0);

    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect4, rect4, SK_ColorBLACK, false);

    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // Since |quad1| and |quad2| are disjoined, no quad can be removed.
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->rect.ToString());
    EXPECT_EQ(rect4.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(1)
                                    ->rect.ToString());
  }

  // +-----++
  // |     ||
  // +-----+|
  // +------+
  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, rect1, is_clipped,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);

    shared_quad_state2->SetAll(gfx::Transform(), rect5, rect5, rect5,
                               is_clipped, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 0);

    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect5, rect5, SK_ColorBLACK, false);

    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // |quad2| extends |quad1|, so |quad1| cannot occlude |quad2|. 2 quads
    // remains in the quad_list.
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->rect.ToString());
    EXPECT_EQ(rect5.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(1)
                                    ->rect.ToString());
  }

  TearDownDisplay();
}

// Check if draw occlusion removes draw quads when quads are being covered
// completely.
TEST_F(DisplayTest, CompositorFrameWithOverlapDrawQuad) {
  SetUpDisplay(RendererSettings(), cc::TestWebGraphicsContext3D::Create());

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());

  CompositorFrame frame = test::MakeCompositorFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(25, 25, 50, 50);
  gfx::Rect rect3(50, 50, 50, 25);
  gfx::Rect rect4(0, 0, 50, 50);

  bool is_clipped = false;
  bool are_contents_opaque = true;
  float opacity = 1.f;
  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.front()
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad2 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();

  // completely overlapping: +-----+
  //                         |     |
  //                         +-----+
  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, rect1, is_clipped,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(gfx::Transform(), rect1, rect1, rect1,
                               is_clipped, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 0);

    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect1, rect1, SK_ColorBLACK, false);
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // |quad2| is completely overlapped with |quad1|, so |quad2| is removed from
    // the |quad_list| and the list size becomes 1.
    EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->rect.ToString());
  }

  //  +-----+
  //  | +-+ |
  //  | +-+ |
  //  +-----+
  {
    quad2 = frame.render_pass_list.front()
                ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, rect1, is_clipped,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(gfx::Transform(), rect2, rect2, rect2,
                               is_clipped, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 0);

    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SK_ColorBLACK, false);
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // |quad2| is in the middle of |quad1| and is completely covered, so |quad2|
    // is removed from the |quad_list| and the list size becomes 1.
    EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->rect.ToString());
  }

  // +-----+
  // |  +--|
  // |  +--|
  // +-----+
  {
    quad2 = frame.render_pass_list.front()
                ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, rect1, is_clipped,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(gfx::Transform(), rect3, rect3, rect3,
                               is_clipped, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 0);

    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect3, rect3, SK_ColorBLACK, false);
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // |quad2| is aligns with |quad1| and is completely covered, so |quad2| is
    // removed from the |quad_list| and the list size becomes 1.
    EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->rect.ToString());
  }

  // +-----++
  // |     ||
  // +-----+|
  // +------+
  {
    quad2 = frame.render_pass_list.front()
                ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, rect1, is_clipped,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);

    shared_quad_state2->SetAll(gfx::Transform(), rect4, rect4, rect4,
                               is_clipped, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 0);

    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect4, rect4, SK_ColorBLACK, false);

    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // |quad2| is covered by |quad 1|, so |quad2| is removed from the
    // |quad_list| and the list size becomes 1.
    EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->rect.ToString());
  }
  TearDownDisplay();
}

// Check if draw occlusion works well with scale change transformer.
TEST_F(DisplayTest, CompositorFrameWithTransformer) {
  SetUpDisplay(RendererSettings(), cc::TestWebGraphicsContext3D::Create());

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());

  // Rect 2, 3, 4 are contained in rect 1 only after applying the scale matrix.
  // They are repetition of the test case above.
  CompositorFrame frame = test::MakeCompositorFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(25, 25, 100, 100);
  gfx::Rect rect3(50, 50, 100, 50);
  gfx::Rect rect4(0, 0, 120, 120);

  // Rect 5, 6, 7 are not contained by rect 1 after applying the scale matrix.
  gfx::Rect rect5(25, 25, 60, 60);
  gfx::Rect rect6(0, 50, 25, 70);
  gfx::Rect rect7(0, 0, 60, 60);

  gfx::Transform half_scale;
  half_scale.Scale3d(0.5, 0.5, 0.5);
  gfx::Transform double_scale;
  double_scale.Scale(2, 2);
  bool is_clipped = false;
  bool are_contents_opaque = true;
  float opacity = 1.f;
  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.front()
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad2 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();

  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, rect1, is_clipped,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(half_scale, rect2, rect2, rect2, is_clipped,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 0);

    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SK_ColorBLACK, false);
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // |rect2| becomes (12, 12, 50x50) after applying scale transformer, |quad2|
    // is now covered by |quad1|. So the size of |quad_list| becomes 1.
    EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->rect.ToString());
  }

  {
    quad2 = frame.render_pass_list.front()
                ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, rect1, is_clipped,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(half_scale, rect3, rect3, rect3, is_clipped,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 0);

    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect3, rect3, SK_ColorBLACK, false);
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // |rect3| becomes (25, 25, 50x25) after applying scale transformer,
    // |quad3| is now covered by |quad1|. So the size of |quad_list| becomes 1.
    EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->rect.ToString());
  }

  {
    quad2 = frame.render_pass_list.front()
                ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, rect1, is_clipped,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);

    shared_quad_state2->SetAll(half_scale, rect4, rect4, rect4, is_clipped,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 0);

    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect4, rect4, SK_ColorBLACK, false);

    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // |rect4| becomes (0, 0, 60x60) after applying scale transformer, |quad4|
    // is now covered by |quad1|. So the size of |quad_list| becomes 1.
    EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->rect.ToString());
  }

  {
    quad2 = frame.render_pass_list.front()
                ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, rect1, is_clipped,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);

    shared_quad_state2->SetAll(double_scale, rect5, rect5, rect5, is_clipped,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 0);

    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect5, rect5, SK_ColorBLACK, false);

    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // |rect5| becomes (50, 50, 120x120) after applying scale transformer,
    // |rect5| is not covered by |rect1|. So the size of |quad_list| is still 2.
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->rect.ToString());
    EXPECT_EQ(rect5.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(1)
                                    ->rect.ToString());
  }

  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, rect1, is_clipped,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);

    shared_quad_state2->SetAll(double_scale, rect6, rect6, rect6, is_clipped,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 0);

    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect6, rect6, SK_ColorBLACK, false);

    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // |rect6| becomes (0, 100, 50x140) after applying scale transformer,
    // |rect6| is not covered by |rect1|. So the size of |quad_list| is still 2.
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->rect.ToString());
    EXPECT_EQ(rect6.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(1)
                                    ->rect.ToString());
  }

  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, rect1, is_clipped,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);

    shared_quad_state2->SetAll(double_scale, rect7, rect7, rect7, is_clipped,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 0);

    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect7, rect7, SK_ColorBLACK, false);

    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // |rect7| becomes (0, 0, 120x120) after applying scale transformer,
    // |rect7| is not covered by |rect1|. So the size of |quad_list| is still 2.
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->rect.ToString());
    EXPECT_EQ(rect7.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(1)
                                    ->rect.ToString());
  }
  TearDownDisplay();
}

// Check if draw occlusion works well with rotation transformer.
//
//  +-----+                                  +----+
//  |     |   rotation (by 45 on y-axis) ->  |    |     same height
//  +-----+                                  +----+     reduced weight
TEST_F(DisplayTest, CompositorFrameWithRotation) {
  SetUpDisplay(RendererSettings(), cc::TestWebGraphicsContext3D::Create());

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());

  // rect 2 is inside rect 1 initially.
  CompositorFrame frame = test::MakeCompositorFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(75, 75, 10, 10);

  gfx::Transform rotate;
  rotate.RotateAboutYAxis(45);
  bool is_clipped = false;
  bool are_contents_opaque = true;
  float opacity = 1.f;
  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.front()
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad2 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  {
    // rect 1 becomes (0, 0, 71x100) after rotation around y-axis, so rect 2 is
    // outside of rect 1 after rotation.
    shared_quad_state->SetAll(rotate, rect1, rect1, rect1, is_clipped,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(gfx::Transform(), rect2, rect2, rect2,
                               is_clipped, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 0);
    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SK_ColorBLACK, false);
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // |rect1| becomes (0, 0, 70x100) after rotation by 45 degree around y-axis,
    // so |rect2| is not covered by |rect1|.
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->rect.ToString());
    EXPECT_EQ(rect2.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(1)
                                    ->rect.ToString());
  }

  {
    // Rotational transformer is applied to both rect 1 and rect 2. So rect 2 is
    // covered by rect 1 after rotation in this case.
    shared_quad_state->SetAll(rotate, rect1, rect1, rect1, is_clipped,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(rotate, rect2, rect2, rect2, is_clipped,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 0);
    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SK_ColorBLACK, false);
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // |rect1| and |rect2| become (0, 0, 70x100) and (53, 75, 8x10) after
    // rotation by 45 degree around y-axis, so |rect2| is covered by |rect1|.
    // |quad_list| is reduced by 1.
    EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->rect.ToString());
  }
  TearDownDisplay();
}

// Check if draw occlusion is handled correctly if the transform does not
// preserves 2d axis alignment.
TEST_F(DisplayTest, CompositorFrameWithPerspective) {
  SetUpDisplay(RendererSettings(), cc::TestWebGraphicsContext3D::Create());

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());

  // rect 2 is inside rect 1 initially.
  CompositorFrame frame = test::MakeCompositorFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(10, 10, 1, 1);

  gfx::Transform perspective;
  perspective.ApplyPerspectiveDepth(100);
  perspective.RotateAboutYAxis(45);

  bool is_clipped = false;
  bool are_contents_opaque = true;
  float opacity = 1.f;
  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.front()
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad2 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  {
    shared_quad_state->SetAll(perspective, rect1, rect1, rect1, is_clipped,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(gfx::Transform(), rect1, rect1, rect1,
                               is_clipped, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 0);
    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect1, rect1, SK_ColorBLACK, false);
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // The transformer after applying rotation and perspective does not perserve
    // 2d axis, so it is hard to find an enclosed rect in the region. The
    // |rect1| cannot be used as occlusion rect to occlude |rect2|. The
    // |quad_list| size remains unchanged.
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->rect.ToString());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(1)
                                    ->rect.ToString());
  }

  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, rect1, is_clipped,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(perspective, rect2, rect2, rect2, is_clipped,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, 0);
    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SK_ColorBLACK, false);
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // |rect2| after applying rotation and perspective does not perserve 2d
    // axis, but it's possible to find a enclosing rect of |rect2| and the
    // resulting rect is occluded by |rect1|. The |quad_list| size is reduced by
    // 1 after calling draw occlusion.
    EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->rect.ToString());
  }
  TearDownDisplay();
}

// Check if draw occlusion works with transparent draw quads.
TEST_F(DisplayTest, CompositorFrameWithOpacityChange) {
  SetUpDisplay(RendererSettings(), cc::TestWebGraphicsContext3D::Create());

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());

  CompositorFrame frame = test::MakeCompositorFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(25, 25, 10, 10);

  bool is_clipped = false;
  bool are_contents_opaque = true;
  float opacity1 = 1.f;
  float opacityLess1 = 0.5f;
  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.front()
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad2 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, rect1, is_clipped,
                              are_contents_opaque, opacityLess1,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(gfx::Transform(), rect2, rect2, rect2,
                               is_clipped, are_contents_opaque, opacity1,
                               SkBlendMode::kSrcOver, 0);
    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SK_ColorBLACK, false);
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // Since the opacity of |rect2| is less than 1, |rect1| cannot occlude
    // |rect2| even though |rect2| is inside |rect1|.
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->rect.ToString());
    EXPECT_EQ(rect2.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(1)
                                    ->rect.ToString());
  }

  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, rect1, is_clipped,
                              are_contents_opaque, opacity1,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(gfx::Transform(), rect2, rect2, rect2,
                               is_clipped, are_contents_opaque, opacity1,
                               SkBlendMode::kSrcOver, 0);
    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SK_ColorBLACK, false);
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // Repeat the above test and set the opacity of |rect1| to 1.
    EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->rect.ToString());
  }

  TearDownDisplay();
}

TEST_F(DisplayTest, CompositorFrameWithOpaquenessChange) {
  SetUpDisplay(RendererSettings(), cc::TestWebGraphicsContext3D::Create());

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());

  CompositorFrame frame = test::MakeCompositorFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(25, 25, 10, 10);

  bool is_clipped = false;
  bool opaque_content = true;
  bool transparent_content = false;
  float opacity = 1.f;
  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.front()
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad2 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, rect1, is_clipped,
                              transparent_content, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(gfx::Transform(), rect2, rect2, rect2,
                               is_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SK_ColorBLACK, false);
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // Since the opaqueness of |rect2| is false, |rect1| cannot occlude
    // |rect2| even though |rect2| is inside |rect1|.
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->rect.ToString());
    EXPECT_EQ(rect2.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(1)
                                    ->rect.ToString());
  }

  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, rect1, is_clipped,
                              opaque_content, opacity, SkBlendMode::kSrcOver,
                              0);
    shared_quad_state2->SetAll(gfx::Transform(), rect2, rect2, rect2,
                               is_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SK_ColorBLACK, false);
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // Repeat the above test and set the opaqueness of |rect2| to true.
    EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->rect.ToString());
  }

  TearDownDisplay();
}

TEST_F(DisplayTest, CompositorFrameWithTranslateTransformer) {
  SetUpDisplay(RendererSettings(), cc::TestWebGraphicsContext3D::Create());

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());

  // rect 2 is outside rect 1 initially.
  CompositorFrame frame = test::MakeCompositorFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(120, 120, 10, 10);

  bool is_clipped = false;
  bool opaque_content = true;
  bool transparent_content = false;
  float opacity = 1.f;
  gfx::Transform translate_up;
  translate_up.Translate(50, 50);
  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.front()
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad2 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  {
    // with transformer is identity matrix, then rect 1 and rect 2 look like:
    //   +----+
    //   |    |
    //   |    |     (move the bigger rect (0, 0) -> (50, 50))         +-----+
    //   +----+                       =>                              | +-+ |
    //           +-+                                                  | +-+ |
    //           +-+                                                  +-----+
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, rect1, is_clipped,
                              transparent_content, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(gfx::Transform(), rect2, rect2, rect2,
                               is_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SK_ColorBLACK, false);
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // |rect2| and |rect1| are disjoined as show in the first image. The size of
    // |quad_list| remains 2.
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->rect.ToString());
    EXPECT_EQ(rect2.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(1)
                                    ->rect.ToString());
  }

  {
    shared_quad_state->SetAll(translate_up, rect1, rect1, rect1, is_clipped,
                              opaque_content, opacity, SkBlendMode::kSrcOver,
                              0);
    shared_quad_state2->SetAll(gfx::Transform(), rect2, rect2, rect2,
                               is_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SK_ColorBLACK, false);
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // Move |rect1| over |rect2| by applying translate to the transformer.
    // |rect2| will be covered by |rect1|, so |quad_list| becomes 1.
    EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->rect.ToString());
  }

  TearDownDisplay();
}

TEST_F(DisplayTest, CompositorFrameWithCombinedSharedQuadState) {
  SetUpDisplay(RendererSettings(), cc::TestWebGraphicsContext3D::Create());

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());

  // rect 3 is inside of combined rect of rect 1 and rect 2.
  CompositorFrame frame = test::MakeCompositorFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(100, 0, 60, 60);
  gfx::Rect rect3(10, 10, 120, 30);

  bool is_clipped = false;
  bool opaque_content = true;
  float opacity = 1.f;
  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.front()
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad2 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state3 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad3 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  {
    //  rect1 & rect2                      rect 3 added
    //   +----+----+                       +----+----+
    //   |    |    |                       |____|___||
    //   |    |----+             =>        |    |----+
    //   +----+                            +----+
    //
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, rect1, is_clipped,
                              opaque_content, opacity, SkBlendMode::kSrcOver,
                              0);
    shared_quad_state2->SetAll(gfx::Transform(), rect2, rect2, rect2,
                               is_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    shared_quad_state3->SetAll(gfx::Transform(), rect3, rect3, rect3,
                               is_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SK_ColorBLACK, false);
    quad3->SetNew(shared_quad_state3, rect3, rect3, SK_ColorBLACK, false);
    EXPECT_EQ(3u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // The occlusion rect is enlarged horizontally after visiting |rect1| and
    // |rect2|. |rect3| is covered by both |rect1| and |rect2|, so |rect3| is
    // removed from |quad_list|.
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->rect.ToString());
    EXPECT_EQ(rect2.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(1)
                                    ->rect.ToString());
  }

  TearDownDisplay();
}

TEST_F(DisplayTest, CompositorFrameWithMultipleRenderPass) {
  SetUpDisplay(RendererSettings(), cc::TestWebGraphicsContext3D::Create());

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());

  // rect 3 is inside of combined rect of rect 1 and rect 2.
  CompositorFrame frame = test::MakeCompositorFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(100, 0, 60, 60);

  std::unique_ptr<RenderPass> render_pass2 = RenderPass::Create();
  render_pass2->SetNew(1, gfx::Rect(), gfx::Rect(), gfx::Transform());
  frame.render_pass_list.push_back(std::move(render_pass2));
  gfx::Rect rect3(10, 10, 120, 30);

  bool is_clipped = false;
  bool opaque_content = true;
  float opacity = 1.f;

  SharedQuadState* shared_quad_state =
      frame.render_pass_list.at(1)->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.at(1)
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.at(1)->CreateAndAppendSharedQuadState();
  auto* quad2 = frame.render_pass_list.at(1)
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state3 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad3 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  {
    // rect1 and rect2 are from first render pass and rect 3 is from the second
    // render pass.
    //  rect1 & rect2                      rect 3 added
    //   +----+----+                       +----+----+
    //   |    |    |                       |____|___||
    //   |    |----+             =>        |    |----+
    //   +----+                            +----+
    //
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, rect1, is_clipped,
                              opaque_content, opacity, SkBlendMode::kSrcOver,
                              0);
    shared_quad_state2->SetAll(gfx::Transform(), rect2, rect2, rect2,
                               is_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    shared_quad_state3->SetAll(gfx::Transform(), rect3, rect3, rect3,
                               is_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SK_ColorBLACK, false);
    quad3->SetNew(shared_quad_state3, rect3, rect3, SK_ColorBLACK, false);
    EXPECT_EQ(2u, frame.render_pass_list.at(1)->quad_list.size());
    EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // The occlusion rect is enlarged horizontally after visiting |rect1| and
    // |rect2|. |rect3| is covered by the unioned region of |rect1| and |rect2|.
    // But |rect3| so |rect3| is to be removed from |quad_list|.
    EXPECT_EQ(2u, frame.render_pass_list.at(1)->quad_list.size());
    EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(
        rect1.ToString(),
        frame.render_pass_list.at(1)->quad_list.ElementAt(0)->rect.ToString());
    EXPECT_EQ(
        rect2.ToString(),
        frame.render_pass_list.at(1)->quad_list.ElementAt(1)->rect.ToString());
    EXPECT_EQ(rect3.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->rect.ToString());
  }
  TearDownDisplay();
}

TEST_F(DisplayTest, CompositorFrameWithCoveredRenderPass) {
  SetUpDisplay(RendererSettings(), cc::TestWebGraphicsContext3D::Create());

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());

  // rect 3 is inside of combined rect of rect 1 and rect 2.
  CompositorFrame frame = test::MakeCompositorFrame();
  gfx::Rect rect1(0, 0, 100, 100);

  std::unique_ptr<RenderPass> render_pass2 = RenderPass::Create();
  render_pass2->SetNew(1, gfx::Rect(), gfx::Rect(), gfx::Transform());
  frame.render_pass_list.push_back(std::move(render_pass2));

  bool is_clipped = false;
  bool opaque_content = true;
  float opacity = 1.f;
  RenderPassId render_pass_id = 1;
  ResourceId mask_resource_id = 2;

  SharedQuadState* shared_quad_state =
      frame.render_pass_list.at(1)->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.at(1)
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.at(1)->CreateAndAppendSharedQuadState();
  auto* quad1 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<RenderPassDrawQuad>();

  {
    {
      // rect1 is a draw quad from SQS1 and which is also the render pass rect
      // from SQS2. renderpassdrawquad should not be occluded.
      //  rect1
      //   +----+
      //   |    |
      //   |    |
      //   +----+
      //

      shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, rect1,
                                is_clipped, opaque_content, opacity,
                                SkBlendMode::kSrcOver, 0);
      shared_quad_state2->SetAll(gfx::Transform(), rect1, rect1, rect1,
                                 is_clipped, opaque_content, opacity,
                                 SkBlendMode::kSrcOver, 0);
      quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
      quad1->SetNew(shared_quad_state2, rect1, rect1, render_pass_id,
                    mask_resource_id, gfx::RectF(), gfx::Size(),
                    gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false);
      EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
      EXPECT_EQ(1u, frame.render_pass_list.at(1)->quad_list.size());
      display_->RemoveOverdrawQuads(&frame);
      // |rect1| and |rect2| shares the same region where |rect1| is a draw
      // quad and |rect2| render pass. |rect2| will be not removed from the
      // |quad_list|.
      EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
      EXPECT_EQ(1u, frame.render_pass_list.at(1)->quad_list.size());
      EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                      ->quad_list.ElementAt(0)
                                      ->rect.ToString());
      EXPECT_EQ(rect1.ToString(), frame.render_pass_list.at(1)
                                      ->quad_list.ElementAt(0)
                                      ->rect.ToString());
    }
  }

  TearDownDisplay();
}

TEST_F(DisplayTest, CompositorFrameWithClip) {
  SetUpDisplay(RendererSettings(), cc::TestWebGraphicsContext3D::Create());

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());

  CompositorFrame frame = test::MakeCompositorFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(50, 50, 25, 25);
  gfx::Rect clip_rect(0, 0, 60, 60);

  bool clipped = true;
  bool non_clipped = false;
  bool opaque_content = true;
  float opacity = 1.f;
  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.front()
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad2 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  {
    // rect1 and rect2 are from first render pass and rect 3 is from the second
    // render pass.
    //  rect1(non-clip) & rect2                rect1(clip) & rect2
    //   +------+                                     +----+
    //   |      |                                     |    |
    //   |   +-+|             =>                      +----+ +-+
    //   +------+                                            +-+
    //
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, rect1,
                              non_clipped, opaque_content, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(gfx::Transform(), rect2, rect2, rect2,
                               non_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SK_ColorBLACK, false);
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // |rect1| covers |rect2| as shown in the image1.So the size of |quad_list|
    // remains unchanged.
    EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->rect.ToString());
  }

  {
    quad2 = frame.render_pass_list.front()
                ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, clip_rect,
                              clipped, opaque_content, opacity,
                              SkBlendMode::kSrcOver, 0);
    shared_quad_state2->SetAll(gfx::Transform(), rect2, rect2, rect2,
                               non_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SK_ColorBLACK, false);
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // |rect1| covers |rect2| as shown in the image1. However, the clip_rect of
    // |rect1| does not cover |rect2|. So the size of |quad_list| is reduced by
    // 1.
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->rect.ToString());
    EXPECT_EQ(rect2.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(1)
                                    ->rect.ToString());
  }
  TearDownDisplay();
}

// Check if draw occlusion works with copy requests in root render pass only.
TEST_F(DisplayTest, CompositorFrameWithCopyRequest) {
  SetUpDisplay(RendererSettings(), cc::TestWebGraphicsContext3D::Create());

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());

  CompositorFrame frame = test::MakeCompositorFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(50, 50, 25, 25);

  bool is_clipped = false;
  bool opaque_content = true;
  float opacity = 1.f;
  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.front()
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad2 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, rect1, is_clipped,
                              opaque_content, opacity, SkBlendMode::kSrcOver,
                              0);
    shared_quad_state2->SetAll(gfx::Transform(), rect2, rect2, rect2,
                               is_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    quad->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SK_ColorBLACK, false);
    frame.render_pass_list.front()->copy_requests.push_back(
        CopyOutputRequest::CreateStubForTesting());
    EXPECT_EQ(2u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // root render pass contains |rect1|, |rect2| and copy_request (where
    // |rect2| is in |rect1|). Since our current implementation only supports
    // occlustion with copy_request on root render pass, |quad_list| reduces its
    // size by 1 after calling remove overdraw.
    EXPECT_EQ(1u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->rect.ToString());
  }
  TearDownDisplay();
}

TEST_F(DisplayTest, CompositorFrameWithRenderPass) {
  SetUpDisplay(RendererSettings(), cc::TestWebGraphicsContext3D::Create());

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());

  CompositorFrame frame = test::MakeCompositorFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(50, 0, 100, 100);
  gfx::Rect rect3(0, 0, 25, 25);
  gfx::Rect rect4(100, 0, 25, 25);
  gfx::Rect rect5(0, 0, 50, 50);
  gfx::Rect rect6(0, 75, 25, 25);
  gfx::Rect rect7(0, 0, 10, 10);

  bool is_clipped = false;
  bool opaque_content = true;
  RenderPassId render_pass_id = 1;
  ResourceId mask_resource_id = 2;
  float opacity = 1.f;
  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* R1 = frame.render_pass_list.front()
                 ->quad_list.AllocateAndConstruct<RenderPassDrawQuad>();
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* R2 = frame.render_pass_list.front()
                 ->quad_list.AllocateAndConstruct<RenderPassDrawQuad>();
  SharedQuadState* shared_quad_state3 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* D1 = frame.render_pass_list.front()
                 ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state4 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* D2 = frame.render_pass_list.front()
                 ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  {
    // Render pass r1 and r2 are intersecting to each other; however, the opaque
    // regions D1 and D2 on R1 and R2 are not intersecting.
    // +-------+---+--------+
    // |_D1_|  |   |_D2_|   |
    // |       |   |        |
    // |   R1  |   |    R2  |
    // +-------+---+--------+
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1, rect1, is_clipped,
                              opaque_content, opacity, SkBlendMode::kSrcOver,
                              0);
    shared_quad_state2->SetAll(gfx::Transform(), rect2, rect2, rect2,
                               is_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    shared_quad_state3->SetAll(gfx::Transform(), rect3, rect3, rect3,
                               is_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    shared_quad_state4->SetAll(gfx::Transform(), rect4, rect4, rect4,
                               is_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    R1->SetNew(shared_quad_state, rect1, rect1, render_pass_id,
               mask_resource_id, gfx::RectF(), gfx::Size(),
               gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false);
    R2->SetNew(shared_quad_state, rect2, rect2, render_pass_id,
               mask_resource_id, gfx::RectF(), gfx::Size(),
               gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false);
    D1->SetNew(shared_quad_state3, rect3, rect3, SK_ColorBLACK, false);
    D2->SetNew(shared_quad_state4, rect4, rect4, SK_ColorBLACK, false);
    EXPECT_EQ(4u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // As shown in the image above, the opaque region |d1| and |d2| does not
    // occlude each other. Since RenderPassDrawQuad |r1| and |r2| cannot be
    // removed to reduce overdraw, |quad_list| remains unchanged.
    EXPECT_EQ(4u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->rect.ToString());
    EXPECT_EQ(rect2.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(1)
                                    ->rect.ToString());
    EXPECT_EQ(rect3.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(2)
                                    ->rect.ToString());
    EXPECT_EQ(rect4.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(3)
                                    ->rect.ToString());
  }

  {
    // Render pass R2 is contained in R1, but the opaque region of the two
    // render passes are separated.
    // +-------+-----------+
    // |_D2_|  |      |_D1_|
    // |       |           |
    // |   R2  |       R1  |
    // +-------+-----------+
    shared_quad_state->SetAll(gfx::Transform(), rect5, rect5, rect5, is_clipped,
                              opaque_content, opacity, SkBlendMode::kSrcOver,
                              0);
    shared_quad_state2->SetAll(gfx::Transform(), rect1, rect1, rect1,
                               is_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    shared_quad_state3->SetAll(gfx::Transform(), rect3, rect3, rect3,
                               is_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    shared_quad_state4->SetAll(gfx::Transform(), rect6, rect6, rect6,
                               is_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    R1->SetNew(shared_quad_state, rect5, rect5, render_pass_id,
               mask_resource_id, gfx::RectF(), gfx::Size(),
               gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false);
    R2->SetNew(shared_quad_state, rect1, rect1, render_pass_id,
               mask_resource_id, gfx::RectF(), gfx::Size(),
               gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false);
    D1->SetNew(shared_quad_state3, rect3, rect3, SK_ColorBLACK, false);
    D2->SetNew(shared_quad_state4, rect6, rect6, SK_ColorBLACK, false);
    EXPECT_EQ(4u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // As shown in the image above, the opaque region |d1| and |d2| does not
    // occlude each other. Since RenderPassDrawQuad |r1| and |r2| cannot be
    // removed to reduce overdraw, |quad_list| remains unchanged.
    EXPECT_EQ(4u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect5.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->rect.ToString());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(1)
                                    ->rect.ToString());
    EXPECT_EQ(rect3.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(2)
                                    ->rect.ToString());
    EXPECT_EQ(rect6.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(3)
                                    ->rect.ToString());
  }

  {
    // Render pass R2 is contained in R1, and opaque region of R2 in R1 as well.
    // +-+---------+-------+
    // |-+   |     |       |
    // |-----+     |       |
    // |   R2      |   R1  |
    // +-----------+-------+
    shared_quad_state->SetAll(gfx::Transform(), rect5, rect5, rect5, is_clipped,
                              opaque_content, opacity, SkBlendMode::kSrcOver,
                              0);
    shared_quad_state2->SetAll(gfx::Transform(), rect1, rect1, rect1,
                               is_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    shared_quad_state3->SetAll(gfx::Transform(), rect3, rect3, rect3,
                               is_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    shared_quad_state4->SetAll(gfx::Transform(), rect7, rect7, rect7,
                               is_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    R1->SetNew(shared_quad_state, rect5, rect5, render_pass_id,
               mask_resource_id, gfx::RectF(), gfx::Size(),
               gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false);
    R2->SetNew(shared_quad_state, rect1, rect1, render_pass_id,
               mask_resource_id, gfx::RectF(), gfx::Size(),
               gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false);
    D1->SetNew(shared_quad_state3, rect3, rect3, SK_ColorBLACK, false);
    D2->SetNew(shared_quad_state4, rect7, rect7, SK_ColorBLACK, false);
    EXPECT_EQ(4u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // As shown in the image above, the opaque region |d2| is contained in |d1|
    // Since RenderPassDrawQuad |r1| and |r2| cannot be removed to reduce
    // overdraw, |quad_list| is reduced by 1.
    EXPECT_EQ(3u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect5.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->rect.ToString());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(1)
                                    ->rect.ToString());
    EXPECT_EQ(rect3.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(2)
                                    ->rect.ToString());
  }
  TearDownDisplay();
}

TEST_F(DisplayTest, CompositorFrameWitMultipleDrawQuadInSharedQuadState) {
  SetUpDisplay(RendererSettings(), cc::TestWebGraphicsContext3D::Create());

  StubDisplayClient client;
  display_->Initialize(&client, manager_.surface_manager());

  CompositorFrame frame = test::MakeCompositorFrame();
  gfx::Rect rect(0, 0, 100, 100);
  gfx::Rect rect1(0, 0, 50, 50);
  gfx::Rect rect2(50, 0, 50, 50);
  gfx::Rect rect3(0, 50, 50, 50);
  gfx::Rect rect4(50, 50, 50, 50);
  gfx::Rect rect5(0, 00, 60, 40);

  bool is_clipped = false;
  bool opaque_content = true;
  float opacity = 1.f;
  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad1 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  auto* quad2 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  auto* quad3 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  auto* quad4 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad5 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();

  {
    // A Shared quad states contains 4 draw quads and it covers another draw
    // quad from different shared quad state.
    // +--+--+
    // +--|+ |
    // +--+--+
    // |  |  |
    // +--+--+
    shared_quad_state->SetAll(gfx::Transform(), rect, rect, rect, is_clipped,
                              opaque_content, opacity, SkBlendMode::kSrcOver,
                              0);
    shared_quad_state2->SetAll(gfx::Transform(), rect5, rect5, rect5,
                               is_clipped, opaque_content, opacity,
                               SkBlendMode::kSrcOver, 0);
    quad1->SetNew(shared_quad_state, rect1, rect1, SK_ColorBLACK, false);
    quad2->SetNew(shared_quad_state, rect2, rect2, SK_ColorBLACK, false);
    quad3->SetNew(shared_quad_state, rect3, rect3, SK_ColorBLACK, false);
    quad4->SetNew(shared_quad_state, rect4, rect4, SK_ColorBLACK, false);
    quad5->SetNew(shared_quad_state2, rect5, rect5, SK_ColorBLACK, false);
    EXPECT_EQ(5u, frame.render_pass_list.front()->quad_list.size());
    display_->RemoveOverdrawQuads(&frame);
    // |visible_rect| of |shared_quad_state| is formed by 4 draw quads and it
    // coved the visible region of |shared_quad_state2|.
    EXPECT_EQ(4u, frame.render_pass_list.front()->quad_list.size());
    EXPECT_EQ(rect1.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(0)
                                    ->rect.ToString());
    EXPECT_EQ(rect2.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(1)
                                    ->rect.ToString());
    EXPECT_EQ(rect3.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(2)
                                    ->rect.ToString());
    EXPECT_EQ(rect4.ToString(), frame.render_pass_list.front()
                                    ->quad_list.ElementAt(3)
                                    ->rect.ToString());
  }
  TearDownDisplay();
}

}  // namespace
}  // namespace viz
