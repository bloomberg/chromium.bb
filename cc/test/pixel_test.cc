// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/pixel_test.h"

#include "base/command_line.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "cc/base/switches.h"
#include "cc/output/compositor_frame_metadata.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "cc/output/gl_renderer.h"
#include "cc/output/output_surface_client.h"
#include "cc/output/software_renderer.h"
#include "cc/resources/raster_worker_pool.h"
#include "cc/resources/resource_provider.h"
#include "cc/resources/texture_mailbox_deleter.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/paths.h"
#include "cc/test/pixel_test_output_surface.h"
#include "cc/test/pixel_test_software_output_device.h"
#include "cc/test/pixel_test_utils.h"
#include "cc/test/test_in_process_context_provider.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

PixelTest::PixelTest()
    : device_viewport_size_(gfx::Size(200, 200)),
      disable_picture_quad_image_filtering_(false),
      output_surface_client_(new FakeOutputSurfaceClient) {}

PixelTest::~PixelTest() {}

bool PixelTest::RunPixelTest(RenderPassList* pass_list,
                             const base::FilePath& ref_file,
                             const PixelComparator& comparator) {
  return RunPixelTestWithReadbackTarget(pass_list,
                                        pass_list->back(),
                                        ref_file,
                                        comparator);
}

bool PixelTest::RunPixelTestWithReadbackTarget(
    RenderPassList* pass_list,
    RenderPass* target,
    const base::FilePath& ref_file,
    const PixelComparator& comparator) {
  base::RunLoop run_loop;

  target->copy_requests.push_back(CopyOutputRequest::CreateBitmapRequest(
      base::Bind(&PixelTest::ReadbackResult,
                 base::Unretained(this),
                 run_loop.QuitClosure())));

  float device_scale_factor = 1.f;
  gfx::Rect device_viewport_rect =
      gfx::Rect(device_viewport_size_) + external_device_viewport_offset_;
  gfx::Rect device_clip_rect = external_device_clip_rect_.IsEmpty()
                                   ? device_viewport_rect
                                   : external_device_clip_rect_;
  renderer_->DecideRenderPassAllocationsForFrame(*pass_list);
  renderer_->DrawFrame(pass_list,
                       device_scale_factor,
                       device_viewport_rect,
                       device_clip_rect,
                       disable_picture_quad_image_filtering_);

  // Wait for the readback to complete.
  if (output_surface_->context_provider())
    output_surface_->context_provider()->ContextGL()->Finish();
  run_loop.Run();

  return PixelsMatchReference(ref_file, comparator);
}

void PixelTest::ReadbackResult(base::Closure quit_run_loop,
                               scoped_ptr<CopyOutputResult> result) {
  ASSERT_TRUE(result->HasBitmap());
  result_bitmap_ = result->TakeBitmap().Pass();
  quit_run_loop.Run();
}

bool PixelTest::PixelsMatchReference(const base::FilePath& ref_file,
                                     const PixelComparator& comparator) {
  base::FilePath test_data_dir;
  if (!PathService::Get(CCPaths::DIR_TEST_DATA, &test_data_dir))
    return false;

  // If this is false, we didn't set up a readback on a render pass.
  if (!result_bitmap_)
    return false;

  CommandLine* cmd = CommandLine::ForCurrentProcess();
  if (cmd->HasSwitch(switches::kCCRebaselinePixeltests))
    return WritePNGFile(*result_bitmap_, test_data_dir.Append(ref_file), true);

  return MatchesPNGFile(
      *result_bitmap_, test_data_dir.Append(ref_file), comparator);
}

void PixelTest::SetUpGLRenderer(bool use_skia_gpu_backend) {
  enable_pixel_output_.reset(new gfx::DisableNullDrawGLBindings);

  output_surface_.reset(
      new PixelTestOutputSurface(new TestInProcessContextProvider));
  output_surface_->BindToClient(output_surface_client_.get());

  shared_bitmap_manager_.reset(new TestSharedBitmapManager());
  resource_provider_ = ResourceProvider::Create(
      output_surface_.get(), shared_bitmap_manager_.get(), 0, false, 1, false);

  texture_mailbox_deleter_ = make_scoped_ptr(
      new TextureMailboxDeleter(base::MessageLoopProxy::current()));

  renderer_ = GLRenderer::Create(this,
                                 &settings_,
                                 output_surface_.get(),
                                 resource_provider_.get(),
                                 texture_mailbox_deleter_.get(),
                                 0).PassAs<DirectRenderer>();
}

void PixelTest::ForceExpandedViewport(const gfx::Size& surface_expansion) {
  static_cast<PixelTestOutputSurface*>(output_surface_.get())
      ->set_surface_expansion_size(surface_expansion);
  SoftwareOutputDevice* device = output_surface_->software_device();
  if (device) {
    static_cast<PixelTestSoftwareOutputDevice*>(device)
        ->set_surface_expansion_size(surface_expansion);
  }
}

void PixelTest::ForceViewportOffset(const gfx::Vector2d& viewport_offset) {
  external_device_viewport_offset_ = viewport_offset;
}

void PixelTest::ForceDeviceClip(const gfx::Rect& clip) {
  external_device_clip_rect_ = clip;
}

void PixelTest::EnableExternalStencilTest() {
  static_cast<PixelTestOutputSurface*>(output_surface_.get())
      ->set_has_external_stencil_test(true);
}

void PixelTest::SetUpSoftwareRenderer() {
  scoped_ptr<SoftwareOutputDevice> device(new PixelTestSoftwareOutputDevice());
  output_surface_.reset(new PixelTestOutputSurface(device.Pass()));
  output_surface_->BindToClient(output_surface_client_.get());
  shared_bitmap_manager_.reset(new TestSharedBitmapManager());
  resource_provider_ = ResourceProvider::Create(
      output_surface_.get(), shared_bitmap_manager_.get(), 0, false, 1, false);
  renderer_ =
      SoftwareRenderer::Create(
          this, &settings_, output_surface_.get(), resource_provider_.get())
          .PassAs<DirectRenderer>();
}

}  // namespace cc
