// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_surface_impl.h"

#include "base/base64.h"
#include "base/test/scoped_feature_list.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/pixel_test_utils.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/service/display_embedder/skia_output_surface_impl.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/ipc/gpu_in_process_thread_service.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "ui/gl/init/gl_factory.h"

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
  void DrawFrameAndVerifyResult(const SkColor output_color,
                                const gfx::Rect& output_rect);

  gfx::Size surface_size_;
  std::unique_ptr<base::Thread> gpu_thread_;
  std::unique_ptr<SkiaOutputSurfaceImpl> output_surface_;
  std::unique_ptr<GpuServiceImpl> gpu_service_;

 private:
  void SetUpSkiaOutputSurfaceImpl();
  void TearDownGpuServiceOnGpuThread();
  void SetUpGpuServiceOnGpuThread();

  std::unique_ptr<base::Thread> io_thread_;
  scoped_refptr<gpu::CommandBufferTaskExecutor> task_executor_;
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

void SkiaOutputSurfaceImplTest::DrawFrameAndVerifyResult(
    const SkColor output_color,
    const gfx::Rect& output_rect) {
  SkCanvas* root_canvas = output_surface_->BeginPaintCurrentFrame();
  SkPaint paint;
  paint.setColor(output_color);
  SkRect rect = SkRect::MakeXYWH(output_rect.x(), output_rect.y(),
                                 output_rect.width(), output_rect.height());
  root_canvas->drawRect(rect, paint);

  gpu::SyncToken sync_token = output_surface_->SubmitPaint();
  EXPECT_TRUE(sync_token.HasData());
  base::OnceClosure closure =
      base::BindOnce(&SkiaOutputSurfaceImplTest::CheckSyncTokenOnGpuThread,
                     base::Unretained(this), sync_token);

  std::vector<gpu::SyncToken> sync_tokens;
  sync_tokens.push_back(sync_token);
  auto sequence_id = gpu_service_->skia_output_surface_sequence_id();
  gpu_service_->scheduler()->ScheduleTask(gpu::Scheduler::Task(
      sequence_id, std::move(closure), std::move(sync_tokens)));
  BlockMainThread();

  // Copy the output
  const gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
  auto request = std::make_unique<CopyOutputRequest>(
      CopyOutputRequest::ResultFormat::RGBA_BITMAP,
      base::BindOnce(&SkiaOutputSurfaceImplTest::CopyRequestCallbackOnGpuThread,
                     base::Unretained(this), output_color, output_rect,
                     color_space));
  request->set_result_task_runner(gpu_thread_->task_runner());
  gfx::Rect result_rect = output_rect;
  output_surface_->CopyOutput(0, output_rect, color_space, result_rect,
                              std::move(request));
  BlockMainThread();
}

void SkiaOutputSurfaceImplTest::SetUpGpuServiceOnGpuThread() {
  ASSERT_TRUE(gpu_thread_->task_runner()->BelongsToCurrentThread());
  gpu_service_ = std::make_unique<GpuServiceImpl>(
      gpu::GPUInfo(), nullptr /* watchdog_thread */, io_thread_->task_runner(),
      gpu::GpuFeatureInfo(), gpu::GpuPreferences(),
      gpu::GPUInfo() /* gpu_info_for_hardware_gpu */,
      gpu::GpuFeatureInfo() /* gpu_feature_info_for_hardware_gpu */,
      nullptr /* vulkan_implementation */,
      base::DoNothing() /* exit_callback */);

  // Uses a null gpu_host here, because we don't want to receive any message.
  std::unique_ptr<mojom::GpuHost> gpu_host;
  mojom::GpuHostPtr gpu_host_proxy;
  mojo::MakeStrongBinding(std::move(gpu_host),
                          mojo::MakeRequest(&gpu_host_proxy));
  gpu_service_->InitializeWithHost(
      std::move(gpu_host_proxy), gpu::GpuProcessActivityFlags(),
      gl::init::CreateOffscreenGLSurface(gfx::Size()),
      nullptr /* sync_point_manager */, nullptr /* shutdown_event */);
  task_executor_ = base::MakeRefCounted<gpu::GpuInProcessThreadService>(
      gpu_thread_->task_runner(), gpu_service_->scheduler(),
      gpu_service_->sync_point_manager(), gpu_service_->mailbox_manager(),
      gpu_service_->share_group(),
      gpu_service_->gpu_channel_manager()
          ->default_offscreen_surface()
          ->GetFormat(),
      gpu_service_->gpu_feature_info(),
      gpu_service_->gpu_channel_manager()->gpu_preferences());
  UnblockMainThread();
}

void SkiaOutputSurfaceImplTest::TearDownGpuServiceOnGpuThread() {
  task_executor_ = nullptr;
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
  const char enable_features[] = "UseSkiaRenderer";
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
      false /* show_overdraw_feedback */);
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

  SkBitmap expected_bitmap;
  expected_bitmap.allocN32Pixels(output_rect.width(), output_rect.height());
  SkCanvas expected_canvas(expected_bitmap);
  SkRect rect;

  SkRect surface_rect;
  surface_rect =
      SkRect::MakeXYWH(0, 0, surface_size_.width(), surface_size_.height());
  bool intersect = surface_rect.intersect(
      SkRect::MakeXYWH(output_rect.x(), output_rect.y(), output_rect.width(),
                       output_rect.height()));

  if (intersect) {
    // When the x y of output_rect are both positive, the left-top of the
    // output_rect remains in output surface, the copy out rect will be filled
    // from left-top (0, 0).
    // When the x y of output_rect are both negative, the left-top of the
    // output_rect  will be shifted beyond the left-top of the output surface,
    // which means the copy out rect is filled from
    // (-output_rect.x(), -output_rect.y()).
    // When the x of output_rect is negative, the left of the output_rect will
    // be shifted beyond the left of the output surface, which means the copy
    // out rect is filled from (-output_rect.x(), 0).
    // When the y of output_rect is negative, the top of the output_rect  will
    // be shifted beyond the top of the output surface, which means the copy
    // out rect is filled from (0, -output_rect.y()).
    if (output_rect.x() < 0 && output_rect.y() < 0)
      rect = SkRect::MakeXYWH(-output_rect.x(), -output_rect.y(),
                              surface_rect.width(), surface_rect.height());
    else if (output_rect.x() < 0)
      rect = SkRect::MakeXYWH(-output_rect.x(), 0, surface_rect.width(),
                              surface_rect.height());
    else if (output_rect.y() < 0)
      rect = SkRect::MakeXYWH(0, -output_rect.y(), surface_rect.width(),
                              surface_rect.height());
    else
      rect = SkRect::MakeWH(surface_rect.width(), surface_rect.height());
  } else {
    rect = SkRect::MakeEmpty();
  }

  SkPaint paint;
  paint.setColor(output_color);
  expected_canvas.drawRect(rect, paint);

  sk_sp<SkPixelRef> pixels(SkSafeRef(expected_bitmap.pixelRef()));
  SkIPoint origin = expected_bitmap.pixelRefOrigin();
  expected_bitmap.setInfo(
      expected_bitmap.info().makeColorSpace(color_space.ToSkColorSpace()),
      expected_bitmap.rowBytes());
  expected_bitmap.setPixelRef(std::move(pixels), origin.x(), origin.y());

  ExpectEquals(*result_bitmap.get(), expected_bitmap);
  UnblockMainThread();
}

TEST_F(SkiaOutputSurfaceImplTest, SubmitPaint) {
  surface_size_ = gfx::Size(100, 100);
  output_surface_->Reshape(surface_size_, 1 /* device_scale_factor */,
                           gfx::ColorSpace(), true /* has_alpha */,
                           false /* use_stencil */);
  DrawFrameAndVerifyResult(SK_ColorRED, gfx::Rect(0, 0, 10, 10));
}

TEST_F(SkiaOutputSurfaceImplTest, Reshape) {
  // The first draw with reshape.
  surface_size_ = gfx::Size(10, 10);
  output_surface_->Reshape(surface_size_, 1 /* device_scale_factor */,
                           gfx::ColorSpace(), true /* has_alpha */,
                           false /* use_stencil */);
  DrawFrameAndVerifyResult(SK_ColorRED, gfx::Rect(0, 0, 10, 10));

  // The second and third draw without reshape.
  DrawFrameAndVerifyResult(SK_ColorGREEN, gfx::Rect(0, 0, 10, 9));
  DrawFrameAndVerifyResult(SK_ColorYELLOW, gfx::Rect(0, 0, 9, 10));

  // The last draw with reshape.
  surface_size_ = gfx::Size(100, 100);
  output_surface_->Reshape(surface_size_, 1 /* device_scale_factor */,
                           gfx::ColorSpace(), true /* has_alpha */,
                           false /* use_stencil */);
  DrawFrameAndVerifyResult(SK_ColorRED, gfx::Rect(0, 0, 20, 20));
}

TEST_F(SkiaOutputSurfaceImplTest, CopyOutOutside) {
  // The first draw, both width and height outside.
  surface_size_ = gfx::Size(100, 100);
  output_surface_->Reshape(surface_size_, 1 /* device_scale_factor */,
                           gfx::ColorSpace(), true /* has_alpha */,
                           false /* use_stencil */);
  DrawFrameAndVerifyResult(SK_ColorRED, gfx::Rect(0, 0, 110, 110));

  // The second draw, height outside.
  output_surface_->Reshape(surface_size_, 1 /* device_scale_factor */,
                           gfx::ColorSpace(), true /* has_alpha */,
                           false /* use_stencil */);
  DrawFrameAndVerifyResult(SK_ColorRED, gfx::Rect(0, 0, 90, 110));

  // The last draw, width outside.
  output_surface_->Reshape(surface_size_, 1 /* device_scale_factor */,
                           gfx::ColorSpace(), true /* has_alpha */,
                           false /* use_stencil */);
  DrawFrameAndVerifyResult(SK_ColorRED, gfx::Rect(0, 0, 110, 90));
}

TEST_F(SkiaOutputSurfaceImplTest, CopyOutOffset) {
  surface_size_ = gfx::Size(100, 100);
  output_surface_->Reshape(surface_size_, 1 /* device_scale_factor */,
                           gfx::ColorSpace(), true /* has_alpha */,
                           false /* use_stencil */);
  DrawFrameAndVerifyResult(SK_ColorRED, gfx::Rect(10, 10, 20, 20));
  DrawFrameAndVerifyResult(SK_ColorRED, gfx::Rect(10, 10, 120, 120));
  DrawFrameAndVerifyResult(SK_ColorRED, gfx::Rect(100, 100, 120, 120));

  DrawFrameAndVerifyResult(SK_ColorRED, gfx::Rect(-10, -10, 20, 20));
  DrawFrameAndVerifyResult(SK_ColorRED, gfx::Rect(-10, 10, 20, 20));
  DrawFrameAndVerifyResult(SK_ColorRED, gfx::Rect(10, -10, 20, 20));
  DrawFrameAndVerifyResult(SK_ColorRED, gfx::Rect(-10, -10, 120, 120));
  DrawFrameAndVerifyResult(SK_ColorRED, gfx::Rect(-10, -10, 0, 0));
}

TEST_F(SkiaOutputSurfaceImplTest, CopyOutSmallSurface) {
  // The  first draw with reshape.
  surface_size_ = gfx::Size(10, 10);
  output_surface_->Reshape(surface_size_, 1 /* device_scale_factor */,
                           gfx::ColorSpace(), true /* has_alpha */,
                           false /* use_stencil */);
  DrawFrameAndVerifyResult(SK_ColorRED, gfx::Rect(0, 0, 10, 10));

  // The last draw without reshape.
  DrawFrameAndVerifyResult(SK_ColorGREEN, gfx::Rect(0, 0, 10, 9));
}

TEST_F(SkiaOutputSurfaceImplTest, CopyOutFullSurface) {
  // The first draw with reshape.
  surface_size_ = gfx::Size(10, 10);
  output_surface_->Reshape(surface_size_, 1 /* device_scale_factor */,
                           gfx::ColorSpace(), true /* has_alpha */,
                           false /* use_stencil */);
  DrawFrameAndVerifyResult(SK_ColorRED, gfx::Rect(0, 0, 10, 10));

  // The last draw without reshape.
  DrawFrameAndVerifyResult(SK_ColorGREEN, gfx::Rect(0, 0, 10, 10));
}

}  // namespace viz
