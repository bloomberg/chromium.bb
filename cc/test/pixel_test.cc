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
#include "cc/raster/raster_buffer_provider.h"
#include "cc/resources/layer_tree_resource_provider.h"
#include "cc/resources/resource_provider.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/pixel_test_output_surface.h"
#include "cc/test/pixel_test_utils.h"
#include "cc/test/test_in_process_context_provider.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "components/viz/service/display/gl_renderer.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/software_output_device.h"
#include "components/viz/service/display/software_renderer.h"
#include "components/viz/test/paths.h"
#include "components/viz/test/test_gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

PixelTest::PixelTest()
    : device_viewport_size_(gfx::Size(200, 200)),
      disable_picture_quad_image_filtering_(false),
      output_surface_client_(new FakeOutputSurfaceClient) {}

PixelTest::~PixelTest() = default;

bool PixelTest::RunPixelTest(viz::RenderPassList* pass_list,
                             const base::FilePath& ref_file,
                             const PixelComparator& comparator) {
  return RunPixelTestWithReadbackTarget(pass_list, pass_list->back().get(),
                                        ref_file, comparator);
}

bool PixelTest::RunPixelTestWithReadbackTarget(
    viz::RenderPassList* pass_list,
    viz::RenderPass* target,
    const base::FilePath& ref_file,
    const PixelComparator& comparator) {
  return RunPixelTestWithReadbackTargetAndArea(
      pass_list, target, ref_file, comparator, nullptr);
}

bool PixelTest::RunPixelTestWithReadbackTargetAndArea(
    viz::RenderPassList* pass_list,
    viz::RenderPass* target,
    const base::FilePath& ref_file,
    const PixelComparator& comparator,
    const gfx::Rect* copy_rect) {
  base::RunLoop run_loop;

  std::unique_ptr<viz::CopyOutputRequest> request =
      std::make_unique<viz::CopyOutputRequest>(
          viz::CopyOutputRequest::ResultFormat::RGBA_BITMAP,
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

bool PixelTest::RunPixelTest(viz::RenderPassList* pass_list,
                             std::vector<SkColor>* ref_pixels,
                             const PixelComparator& comparator) {
  base::RunLoop run_loop;
  viz::RenderPass* target = pass_list->back().get();

  std::unique_ptr<viz::CopyOutputRequest> request =
      std::make_unique<viz::CopyOutputRequest>(
          viz::CopyOutputRequest::ResultFormat::RGBA_BITMAP,
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
  ASSERT_FALSE(result->IsEmpty());
  EXPECT_EQ(result->format(), viz::CopyOutputResult::Format::RGBA_BITMAP);
  result_bitmap_ = std::make_unique<SkBitmap>(result->AsSkBitmap());
  EXPECT_TRUE(result_bitmap_->readyToDraw());
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

void PixelTest::SetUpGLWithoutRenderer(bool flipped_output_surface) {
  enable_pixel_output_ = std::make_unique<gl::DisableNullDrawGLBindings>();

  auto context_provider =
      base::MakeRefCounted<TestInProcessContextProvider>(nullptr);
  output_surface_ = std::make_unique<PixelTestOutputSurface>(
      std::move(context_provider), flipped_output_surface);
  output_surface_->BindToClient(output_surface_client_.get());

  shared_bitmap_manager_ = std::make_unique<TestSharedBitmapManager>();
  gpu_memory_buffer_manager_ =
      std::make_unique<viz::TestGpuMemoryBufferManager>();
  resource_provider_ = std::make_unique<DisplayResourceProvider>(
      output_surface_->context_provider(), shared_bitmap_manager_.get(),
      gpu_memory_buffer_manager_.get(), settings_.resource_settings);

  child_context_provider_ =
      base::MakeRefCounted<TestInProcessContextProvider>(nullptr);
  child_context_provider_->BindToCurrentThread();
  child_resource_provider_ = std::make_unique<LayerTreeResourceProvider>(
      child_context_provider_.get(), shared_bitmap_manager_.get(),
      gpu_memory_buffer_manager_.get(), true,
      settings_.resource_settings);
}

void PixelTest::SetUpGLRenderer(bool flipped_output_surface) {
  SetUpGLWithoutRenderer(flipped_output_surface);
  renderer_ = std::make_unique<viz::GLRenderer>(
      &renderer_settings_, output_surface_.get(), resource_provider_.get(),
      base::ThreadTaskRunnerHandle::Get());
  renderer_->Initialize();
  renderer_->SetVisible(true);
}

void PixelTest::EnableExternalStencilTest() {
  static_cast<PixelTestOutputSurface*>(output_surface_.get())
      ->set_has_external_stencil_test(true);
}

void PixelTest::SetUpSoftwareRenderer() {
  output_surface_.reset(new PixelTestOutputSurface(
      std::make_unique<viz::SoftwareOutputDevice>()));
  output_surface_->BindToClient(output_surface_client_.get());
  shared_bitmap_manager_.reset(new TestSharedBitmapManager());
  resource_provider_ = std::make_unique<DisplayResourceProvider>(
      nullptr, shared_bitmap_manager_.get(), gpu_memory_buffer_manager_.get(),
      settings_.resource_settings);
  auto renderer = std::make_unique<viz::SoftwareRenderer>(
      &renderer_settings_, output_surface_.get(), resource_provider_.get());
  software_renderer_ = renderer.get();
  renderer_ = std::move(renderer);
  renderer_->Initialize();
  renderer_->SetVisible(true);
}

}  // namespace cc
