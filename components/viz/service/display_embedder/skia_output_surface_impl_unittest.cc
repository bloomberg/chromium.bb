// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_surface_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/pixel_test_utils.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/frame_sinks/copy_output_util.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/ipc/gpu_in_process_thread_service.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "gpu/vulkan/buildflags.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/init/gl_factory.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "gpu/vulkan/init/vulkan_factory.h"
#include "gpu/vulkan/vulkan_implementation.h"
#endif

namespace viz {

static void ExpectEquals(SkBitmap actual, SkBitmap expected) {
  EXPECT_EQ(actual.dimensions(), expected.dimensions());
  auto expected_url = cc::GetPNGDataUrl(expected);
  auto actual_url = cc::GetPNGDataUrl(actual);
  EXPECT_TRUE(actual_url == expected_url);
}

class SkiaOutputSurfaceImplTest : public testing::Test {
 public:
  void CheckSyncTokenOnGpuThread(const gpu::SyncToken& sync_token);
  void CopyRequestCallbackOnGpuThread(const SkColor output_color,
                                      const gfx::Rect& output_rect,
                                      const gfx::ColorSpace& color_space,
                                      std::unique_ptr<CopyOutputResult> result);

 protected:
  SkiaOutputSurfaceImplTest()
      : output_surface_client_(std::make_unique<cc::FakeOutputSurfaceClient>()),
        wait_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
              base::WaitableEvent::InitialState::NOT_SIGNALED) {}
  inline void SetUp() override { SetUpSkiaOutputSurfaceImpl(); }
  void TearDown() override;
  void BlockMainThread();
  void UnblockMainThread();
  bool is_vulkan_enabled() {
#if BUILDFLAG(ENABLE_VULKAN)
    return !!vulkan_implementation_;
#else
    return false;
#endif
  }

  std::unique_ptr<base::Thread> gpu_thread_;
  std::unique_ptr<SkiaOutputSurfaceImpl> output_surface_;
  std::unique_ptr<GpuServiceImpl> gpu_service_;

 private:
  void SetUpSkiaOutputSurfaceImpl();
  void TearDownGpuServiceOnGpuThread();
  void SetUpGpuServiceOnGpuThread();

#if BUILDFLAG(ENABLE_VULKAN)
  std::unique_ptr<gpu::VulkanImplementation> vulkan_implementation_;
#endif

  std::unique_ptr<base::Thread> io_thread_;
  std::unique_ptr<gpu::CommandBufferTaskExecutor> task_executor_;
  std::unique_ptr<cc::FakeOutputSurfaceClient> output_surface_client_;
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
  base::WaitableEvent wait_;
};

void SkiaOutputSurfaceImplTest::BlockMainThread() {
  wait_.Wait();
}

void SkiaOutputSurfaceImplTest::UnblockMainThread() {
  DCHECK(!wait_.IsSignaled());
  wait_.Signal();
}

void SkiaOutputSurfaceImplTest::SetUpGpuServiceOnGpuThread() {
  ASSERT_TRUE(gpu_thread_->task_runner()->BelongsToCurrentThread());
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  gpu::GpuPreferences gpu_preferences =
      gpu::gles2::ParseGpuPreferences(command_line);
  if (gpu_preferences.enable_vulkan) {
#if BUILDFLAG(ENABLE_VULKAN)
    vulkan_implementation_ = gpu::CreateVulkanImplementation();
    if (!vulkan_implementation_ ||
        !vulkan_implementation_->InitializeVulkanInstance()) {
      LOG(FATAL) << "Failed to create and initialize Vulkan implementation.";
    }
#else
    NOTREACHED();
#endif
  }
  gpu_service_ = std::make_unique<GpuServiceImpl>(
      gpu::GPUInfo(), nullptr /* watchdog_thread */, io_thread_->task_runner(),
      gpu::GpuFeatureInfo(), gpu_preferences,
      gpu::GPUInfo() /* gpu_info_for_hardware_gpu */,
      gpu::GpuFeatureInfo() /* gpu_feature_info_for_hardware_gpu */,
#if BUILDFLAG(ENABLE_VULKAN)
      vulkan_implementation_.get(),
#else
      nullptr /* vulkan_implementation */,
#endif
      base::DoNothing() /* exit_callback */);

  // Uses a null gpu_host here, because we don't want to receive any message.
  std::unique_ptr<mojom::GpuHost> gpu_host;
  mojom::GpuHostPtr gpu_host_proxy;
  mojo::MakeStrongBinding(std::move(gpu_host),
                          mojo::MakeRequest(&gpu_host_proxy));
  gpu_service_->InitializeWithHost(
      std::move(gpu_host_proxy), gpu::GpuProcessActivityFlags(),
      gl::init::CreateOffscreenGLSurface(gfx::Size()),
      nullptr /* sync_point_manager */, nullptr /* shared_image_manager */,
      nullptr /* shutdown_event */);
  task_executor_ = std::make_unique<gpu::GpuInProcessThreadService>(
      gpu_thread_->task_runner(), gpu_service_->scheduler(),
      gpu_service_->sync_point_manager(), gpu_service_->mailbox_manager(),
      gpu_service_->share_group(),
      gpu_service_->gpu_channel_manager()
          ->default_offscreen_surface()
          ->GetFormat(),
      gpu_service_->gpu_feature_info(),
      gpu_service_->gpu_channel_manager()->gpu_preferences(),
      gpu_service_->shared_image_manager(),
      gpu_service_->gpu_channel_manager()->program_cache());
  UnblockMainThread();
}

void SkiaOutputSurfaceImplTest::TearDownGpuServiceOnGpuThread() {
  task_executor_.reset();
  gpu_service_ = nullptr;
  UnblockMainThread();
}

void SkiaOutputSurfaceImplTest::TearDown() {
  output_surface_ = nullptr;

  if (task_executor_) {
    // Tear down the GPU service.
    gpu_thread_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &SkiaOutputSurfaceImplTest::TearDownGpuServiceOnGpuThread,
            base::Unretained(this)));
    BlockMainThread();
  }
  io_thread_ = nullptr;
  gpu_thread_ = nullptr;
  scoped_feature_list_ = nullptr;
}

void SkiaOutputSurfaceImplTest::SetUpSkiaOutputSurfaceImpl() {
  // SkiaOutputSurfaceImplOnGpu requires UseSkiaRenderer.
  const char enable_features[] = "VizDisplayCompositor,UseSkiaRenderer";
  const char disable_features[] = "";
  scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list_->InitFromCommandLine(enable_features, disable_features);

  // Set up the GPU service.
  gpu_thread_ = std::make_unique<base::Thread>("GPUMainThread");
  ASSERT_TRUE(gpu_thread_->Start());
  io_thread_ = std::make_unique<base::Thread>("GPUIOThread");
  ASSERT_TRUE(io_thread_->Start());

  gpu_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&SkiaOutputSurfaceImplTest::SetUpGpuServiceOnGpuThread,
                     base::Unretained(this)));
  BlockMainThread();

  // Set up the SkiaOutputSurfaceImpl.
  output_surface_ = std::make_unique<SkiaOutputSurfaceImpl>(
      gpu_service_.get(), gpu::kNullSurfaceHandle,
      nullptr /* synthetic_begin_frame_source */,
      false /*show_overdraw_feedback*/);
  output_surface_->BindToClient(output_surface_client_.get());
}

void SkiaOutputSurfaceImplTest::CheckSyncTokenOnGpuThread(
    const gpu::SyncToken& sync_token) {
  EXPECT_TRUE(
      gpu_service_->sync_point_manager()->IsSyncTokenReleased(sync_token));
  UnblockMainThread();
}

void SkiaOutputSurfaceImplTest::CopyRequestCallbackOnGpuThread(
    const SkColor output_color,
    const gfx::Rect& output_rect,
    const gfx::ColorSpace& color_space,
    std::unique_ptr<CopyOutputResult> result) {
  std::unique_ptr<SkBitmap> result_bitmap;
  result_bitmap = std::make_unique<SkBitmap>(result->AsSkBitmap());
  EXPECT_EQ(result_bitmap->width(), output_rect.width());
  EXPECT_EQ(result_bitmap->height(), output_rect.height());

  std::vector<SkPMColor> expected_pixels(
      output_rect.width() * output_rect.height(),
      SkPreMultiplyColor(output_color));
  SkBitmap expected;
  expected.installPixels(
      SkImageInfo::MakeN32Premul(output_rect.width(), output_rect.height(),
                                 color_space.ToSkColorSpace()),
      expected_pixels.data(), output_rect.width() * sizeof(SkColor));
  ExpectEquals(*result_bitmap.get(), expected);

  UnblockMainThread();
}

TEST_F(SkiaOutputSurfaceImplTest, SubmitPaint) {
  const gfx::Rect surface_rect(0, 0, 100, 100);
  output_surface_->Reshape(surface_rect.size(), 1, gfx::ColorSpace(), true,
                           false);
  SkCanvas* root_canvas = output_surface_->BeginPaintCurrentFrame();
  SkPaint paint;
  const SkColor output_color = SK_ColorRED;
  const gfx::Rect output_rect(0, 0, 10, 10);
  paint.setColor(output_color);
  SkRect rect = SkRect::MakeWH(output_rect.width(), output_rect.height());
  root_canvas->drawRect(rect, paint);

  gpu::SyncToken sync_token = output_surface_->SubmitPaint();
  EXPECT_TRUE(sync_token.HasData());
  base::OnceClosure closure =
      base::BindOnce(&SkiaOutputSurfaceImplTest::CheckSyncTokenOnGpuThread,
                     base::Unretained(this), sync_token);

  std::vector<gpu::SyncToken> resource_sync_tokens;
  resource_sync_tokens.push_back(sync_token);
  auto sequence_id = gpu_service_->skia_output_surface_sequence_id();
  gpu_service_->scheduler()->ScheduleTask(gpu::Scheduler::Task(
      sequence_id, std::move(closure), std::move(resource_sync_tokens)));
  BlockMainThread();

  // Copy the output
  const gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
  auto request = std::make_unique<CopyOutputRequest>(
      CopyOutputRequest::ResultFormat::RGBA_BITMAP,
      base::BindOnce(&SkiaOutputSurfaceImplTest::CopyRequestCallbackOnGpuThread,
                     base::Unretained(this), output_color, output_rect,
                     color_space));
  request->set_result_task_runner(gpu_thread_->task_runner());
  copy_output::RenderPassGeometry geometry;
  geometry.result_bounds = surface_rect;
  geometry.result_selection = output_rect;
  geometry.sampling_bounds = surface_rect;

  if (is_vulkan_enabled()) {
    // No flipping because Skia handles all co-ordinate transformation on the
    // software readback path currently implemented for Vulkan.
    geometry.readback_offset = geometry.readback_offset = gfx::Vector2d(0, 0);
  } else {
    // GLRendererCopier may need a vertical flip depending on output surface
    // characteristics.
    geometry.readback_offset =
        output_surface_->capabilities().flipped_output_surface
            ? geometry.readback_offset = gfx::Vector2d(0, 0)
            : geometry.readback_offset = gfx::Vector2d(0, 90);
  }
  output_surface_->CopyOutput(0, geometry, color_space, std::move(request));
  BlockMainThread();
}

}  // namespace viz
