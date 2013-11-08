// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/pixel_test.h"

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "cc/base/switches.h"
#include "cc/output/compositor_frame_metadata.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "cc/output/gl_renderer.h"
#include "cc/output/output_surface_client.h"
#include "cc/output/software_renderer.h"
#include "cc/resources/resource_provider.h"
#include "cc/resources/texture_mailbox_deleter.h"
#include "cc/test/paths.h"
#include "cc/test/pixel_test_output_surface.h"
#include "cc/test/pixel_test_software_output_device.h"
#include "cc/test/pixel_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_implementation.h"
#include "webkit/common/gpu/context_provider_in_process.h"
#include "webkit/common/gpu/webgraphicscontext3d_in_process_command_buffer_impl.h"

namespace cc {

class PixelTest::PixelTestRendererClient
    : public RendererClient, public OutputSurfaceClient {
 public:
  explicit PixelTestRendererClient(gfx::Rect device_viewport)
      : device_viewport_(device_viewport), device_clip_(device_viewport) {}

  // RendererClient implementation.
  virtual gfx::Rect DeviceViewport() const OVERRIDE {
    return device_viewport_;
  }
  virtual gfx::Rect DeviceClip() const OVERRIDE { return device_clip_; }
  virtual void SetFullRootLayerDamage() OVERRIDE {}
  virtual CompositorFrameMetadata MakeCompositorFrameMetadata() const
      OVERRIDE {
    return CompositorFrameMetadata();
  }

  // OutputSurfaceClient implementation.
  virtual bool DeferredInitialize(
      scoped_refptr<ContextProvider> offscreen_context_provider) OVERRIDE {
    return false;
  }
  virtual void ReleaseGL() OVERRIDE {}
  virtual void SetNeedsRedrawRect(gfx::Rect damage_rect) OVERRIDE {}
  virtual void BeginImplFrame(const BeginFrameArgs& args) OVERRIDE {}
  virtual void DidSwapBuffers() OVERRIDE {}
  virtual void OnSwapBuffersComplete() OVERRIDE {}
  virtual void ReclaimResources(const CompositorFrameAck* ack) OVERRIDE {}
  virtual void DidLoseOutputSurface() OVERRIDE {}
  virtual void SetExternalDrawConstraints(
      const gfx::Transform& transform,
      gfx::Rect viewport,
      gfx::Rect clip,
      bool valid_for_tile_management) OVERRIDE {
    device_viewport_ = viewport;
    device_clip_ = clip;
  }
  virtual void SetMemoryPolicy(const ManagedMemoryPolicy& policy) OVERRIDE {}
  virtual void SetTreeActivationCallback(const base::Closure&) OVERRIDE {}

 private:
  gfx::Rect device_viewport_;
  gfx::Rect device_clip_;
};

PixelTest::PixelTest()
    : device_viewport_size_(gfx::Size(200, 200)),
      disable_picture_quad_image_filtering_(false),
      fake_client_(
          new PixelTestRendererClient(gfx::Rect(device_viewport_size_))) {}

PixelTest::~PixelTest() {}

bool PixelTest::RunPixelTest(RenderPassList* pass_list,
                             OffscreenContextOption provide_offscreen_context,
                             const base::FilePath& ref_file,
                             const PixelComparator& comparator) {
  return RunPixelTestWithReadbackTarget(pass_list,
                                        pass_list->back(),
                                        provide_offscreen_context,
                                        ref_file,
                                        comparator);
}

bool PixelTest::RunPixelTestWithReadbackTarget(
    RenderPassList* pass_list,
    RenderPass* target,
    OffscreenContextOption provide_offscreen_context,
    const base::FilePath& ref_file,
    const PixelComparator& comparator) {
  base::RunLoop run_loop;

  target->copy_requests.push_back(CopyOutputRequest::CreateBitmapRequest(
      base::Bind(&PixelTest::ReadbackResult,
                 base::Unretained(this),
                 run_loop.QuitClosure())));

  scoped_refptr<webkit::gpu::ContextProviderInProcess> offscreen_contexts;
  switch (provide_offscreen_context) {
    case NoOffscreenContext:
      break;
    case WithOffscreenContext:
      offscreen_contexts =
          webkit::gpu::ContextProviderInProcess::CreateOffscreen();
      CHECK(offscreen_contexts->BindToCurrentThread());
      break;
  }

  float device_scale_factor = 1.f;
  bool allow_partial_swap = true;

  renderer_->DecideRenderPassAllocationsForFrame(*pass_list);
  renderer_->DrawFrame(pass_list,
                       offscreen_contexts.get(),
                       device_scale_factor,
                       allow_partial_swap,
                       disable_picture_quad_image_filtering_);

  // Wait for the readback to complete.
  resource_provider_->Finish();
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
  if (!PathService::Get(cc::DIR_TEST_DATA, &test_data_dir))
    return false;

  // If this is false, we didn't set up a readback on a render pass.
  if (!result_bitmap_)
    return false;

  CommandLine* cmd = CommandLine::ForCurrentProcess();
  if (cmd->HasSwitch(switches::kCCRebaselinePixeltests))
    return WritePNGFile(*result_bitmap_, test_data_dir.Append(ref_file), true);

  return MatchesPNGFile(*result_bitmap_,
                        test_data_dir.Append(ref_file),
                        comparator);
}

void PixelTest::SetUpGLRenderer(bool use_skia_gpu_backend) {
  CHECK(fake_client_);
  CHECK(gfx::InitializeGLBindings(gfx::kGLImplementationOSMesaGL));

  using webkit::gpu::ContextProviderInProcess;
  output_surface_.reset(new PixelTestOutputSurface(
      ContextProviderInProcess::CreateOffscreen()));
  output_surface_->BindToClient(fake_client_.get());

  resource_provider_ =
      ResourceProvider::Create(output_surface_.get(), NULL, 0, false, 1);

  texture_mailbox_deleter_ = make_scoped_ptr(new TextureMailboxDeleter);

  renderer_ = GLRenderer::Create(fake_client_.get(),
                                 &settings_,
                                 output_surface_.get(),
                                 resource_provider_.get(),
                                 texture_mailbox_deleter_.get(),
                                 0,
                                 use_skia_gpu_backend).PassAs<DirectRenderer>();
}

void PixelTest::ForceExpandedViewport(gfx::Size surface_expansion,
                                      gfx::Vector2d viewport_offset) {
  static_cast<PixelTestOutputSurface*>(output_surface_.get())
      ->set_surface_expansion_size(surface_expansion);
  static_cast<PixelTestOutputSurface*>(output_surface_.get())
      ->set_viewport_offset(viewport_offset);
  SoftwareOutputDevice* device = output_surface_->software_device();
  if (device) {
    static_cast<PixelTestSoftwareOutputDevice*>(device)
        ->set_surface_expansion_size(surface_expansion);
  }
}

void PixelTest::ForceDeviceClip(gfx::Rect clip) {
  static_cast<PixelTestOutputSurface*>(output_surface_.get())
      ->set_device_clip(clip);
}

void PixelTest::EnableExternalStencilTest() {
  static_cast<PixelTestOutputSurface*>(output_surface_.get())
      ->set_has_external_stencil_test(true);
}

void PixelTest::SetUpSoftwareRenderer() {
  CHECK(fake_client_);

  scoped_ptr<SoftwareOutputDevice> device(new PixelTestSoftwareOutputDevice());
  output_surface_.reset(new PixelTestOutputSurface(device.Pass()));
  output_surface_->BindToClient(fake_client_.get());
  resource_provider_ =
      ResourceProvider::Create(output_surface_.get(), NULL, 0, false, 1);
  renderer_ = SoftwareRenderer::Create(fake_client_.get(),
                                       &settings_,
                                       output_surface_.get(),
                                       resource_provider_.get())
      .PassAs<DirectRenderer>();
}

}  // namespace cc
