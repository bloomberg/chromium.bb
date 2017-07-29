// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/pixel_test.h"

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/base/switches.h"
#include "cc/output/compositor_frame_metadata.h"
#include "cc/output/output_surface_client.h"
#include "cc/output/software_output_device.h"
#include "cc/output/software_renderer.h"
#include "cc/output/texture_mailbox_deleter.h"
#include "cc/raster/raster_buffer_provider.h"
#include "cc/resources/resource_provider.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/pixel_test_output_surface.h"
#include "cc/test/pixel_test_utils.h"
#include "cc/test/test_gpu_memory_buffer_manager.h"
#include "cc/test/test_in_process_context_provider.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/trees/blocking_task_runner.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/quads/copy_output_request.h"
#include "components/viz/common/quads/copy_output_result.h"
#include "components/viz/service/display/gl_renderer.h"
#include "components/viz/test/paths.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

PixelTest::PixelTest()
    : device_viewport_size_(gfx::Size(200, 200)),
      disable_picture_quad_image_filtering_(false),
      output_surface_client_(new FakeOutputSurfaceClient),
      main_thread_task_runner_(
          BlockingTaskRunner::Create(base::ThreadTaskRunnerHandle::Get())) {}
PixelTest::~PixelTest() {}

bool PixelTest::RunPixelTest(RenderPassList* pass_list,
                             const base::FilePath& ref_file,
                             const PixelComparator& comparator) {
  return RunPixelTestWithReadbackTarget(pass_list, pass_list->back().get(),
                                        ref_file, comparator);
}

bool PixelTest::RunPixelTestWithReadbackTarget(
    RenderPassList* pass_list,
    RenderPass* target,
    const base::FilePath& ref_file,
    const PixelComparator& comparator) {
  return RunPixelTestWithReadbackTargetAndArea(
      pass_list, target, ref_file, comparator, nullptr);
}

bool PixelTest::RunPixelTestWithReadbackTargetAndArea(
    RenderPassList* pass_list,
    RenderPass* target,
    const base::FilePath& ref_file,
    const PixelComparator& comparator,
    const gfx::Rect* copy_rect) {
  base::RunLoop run_loop;

  std::unique_ptr<viz::CopyOutputRequest> request =
      viz::CopyOutputRequest::CreateBitmapRequest(
          base::BindOnce(&PixelTest::ReadbackResult, base::Unretained(this),
                         run_loop.QuitClosure()));
  if (copy_rect)
    request->set_area(*copy_rect);
  target->copy_requests.push_back(std::move(request));

  if (software_renderer_) {
    software_renderer_->SetDisablePictureQuadImageFiltering(
        disable_picture_quad_image_filtering_);
  }

  renderer_->DecideRenderPassAllocationsForFrame(*pass_list);
  float device_scale_factor = 1.f;
  renderer_->DrawFrame(pass_list, device_scale_factor, device_viewport_size_);

  // Wait for the readback to complete.
  if (output_surface_->context_provider())
    output_surface_->context_provider()->ContextGL()->Finish();
  run_loop.Run();

  return PixelsMatchReference(ref_file, comparator);
}

bool PixelTest::RunPixelTest(RenderPassList* pass_list,
                             std::vector<SkColor>* ref_pixels,
                             const PixelComparator& comparator) {
  base::RunLoop run_loop;
  RenderPass* target = pass_list->back().get();

  std::unique_ptr<viz::CopyOutputRequest> request =
      viz::CopyOutputRequest::CreateBitmapRequest(
          base::BindOnce(&PixelTest::ReadbackResult, base::Unretained(this),
                         run_loop.QuitClosure()));
  target->copy_requests.push_back(std::move(request));

  if (software_renderer_) {
    software_renderer_->SetDisablePictureQuadImageFiltering(
        disable_picture_quad_image_filtering_);
  }

  renderer_->DecideRenderPassAllocationsForFrame(*pass_list);
  float device_scale_factor = 1.f;
  renderer_->DrawFrame(pass_list, device_scale_factor, device_viewport_size_);

  // Wait for the readback to complete.
  if (output_surface_->context_provider())
    output_surface_->context_provider()->ContextGL()->Finish();
  run_loop.Run();

  // Need to wrap |ref_pixels| in a SkBitmap.
  DCHECK_EQ(ref_pixels->size(), static_cast<size_t>(result_bitmap_->width() *
                                                    result_bitmap_->height()));
  SkBitmap ref_pixels_bitmap;
  ref_pixels_bitmap.installPixels(
      SkImageInfo::MakeN32Premul(result_bitmap_->width(),
                                 result_bitmap_->height()),
      ref_pixels->data(), result_bitmap_->width() * sizeof(SkColor));
  return comparator.Compare(*result_bitmap_, ref_pixels_bitmap);
}

void PixelTest::ReadbackResult(base::Closure quit_run_loop,
                               std::unique_ptr<viz::CopyOutputResult> result) {
  ASSERT_TRUE(result->HasBitmap());
  result_bitmap_ = result->TakeBitmap();
  quit_run_loop.Run();
}

bool PixelTest::PixelsMatchReference(const base::FilePath& ref_file,
                                     const PixelComparator& comparator) {
  base::FilePath test_data_dir;
  if (!PathService::Get(viz::Paths::DIR_TEST_DATA, &test_data_dir))
    return false;

  // If this is false, we didn't set up a readback on a render pass.
  if (!result_bitmap_)
    return false;

  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  if (cmd->HasSwitch(switches::kCCRebaselinePixeltests))
    return WritePNGFile(*result_bitmap_, test_data_dir.Append(ref_file), true);

  return MatchesPNGFile(
      *result_bitmap_, test_data_dir.Append(ref_file), comparator);
}

void PixelTest::SetUpGLRenderer(bool use_skia_gpu_backend,
                                bool flipped_output_surface) {
  enable_pixel_output_.reset(new gl::DisableNullDrawGLBindings);

  scoped_refptr<TestInProcessContextProvider> context_provider(
      new TestInProcessContextProvider(nullptr));
  output_surface_.reset(new PixelTestOutputSurface(std::move(context_provider),
                                                   flipped_output_surface));
  output_surface_->BindToClient(output_surface_client_.get());

  shared_bitmap_manager_.reset(new TestSharedBitmapManager);
  gpu_memory_buffer_manager_.reset(new TestGpuMemoryBufferManager);
  // Not relevant for display compositor since it's not delegated.
  constexpr bool delegated_sync_points_required = false;
  resource_provider_ = base::MakeUnique<ResourceProvider>(
      output_surface_->context_provider(), shared_bitmap_manager_.get(),
      gpu_memory_buffer_manager_.get(), main_thread_task_runner_.get(),
      delegated_sync_points_required,
      settings_.enable_color_correct_rasterization,
      settings_.resource_settings);

  texture_mailbox_deleter_ = base::MakeUnique<TextureMailboxDeleter>(
      base::ThreadTaskRunnerHandle::Get());

  renderer_ = base::MakeUnique<viz::GLRenderer>(
      &renderer_settings_, output_surface_.get(), resource_provider_.get(),
      texture_mailbox_deleter_.get());
  renderer_->Initialize();
  renderer_->SetVisible(true);
}

void PixelTest::EnableExternalStencilTest() {
  static_cast<PixelTestOutputSurface*>(output_surface_.get())
      ->set_has_external_stencil_test(true);
}

void PixelTest::SetUpSoftwareRenderer() {
  output_surface_.reset(
      new PixelTestOutputSurface(base::MakeUnique<SoftwareOutputDevice>()));
  output_surface_->BindToClient(output_surface_client_.get());
  shared_bitmap_manager_.reset(new TestSharedBitmapManager());
  constexpr bool delegated_sync_points_required =
      false;  // Meaningless for software.
  resource_provider_ = base::MakeUnique<ResourceProvider>(
      nullptr, shared_bitmap_manager_.get(), gpu_memory_buffer_manager_.get(),
      main_thread_task_runner_.get(), delegated_sync_points_required,
      settings_.enable_color_correct_rasterization,
      settings_.resource_settings);
  auto renderer = base::MakeUnique<SoftwareRenderer>(
      &renderer_settings_, output_surface_.get(), resource_provider_.get());
  software_renderer_ = renderer.get();
  renderer_ = std::move(renderer);
  renderer_->Initialize();
  renderer_->SetVisible(true);
}

}  // namespace cc
