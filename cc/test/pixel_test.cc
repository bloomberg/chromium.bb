// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/pixel_test.h"

#include "base/path_service.h"
#include "base/run_loop.h"
#include "cc/output/compositor_frame_metadata.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "cc/output/gl_renderer.h"
#include "cc/output/output_surface_client.h"
#include "cc/output/software_renderer.h"
#include "cc/resources/resource_provider.h"
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
      : device_viewport_(device_viewport), stencil_enabled_(false) {}

  // RendererClient implementation.
  virtual gfx::Rect DeviceViewport() const OVERRIDE {
    return device_viewport_;
  }
  virtual float DeviceScaleFactor() const OVERRIDE {
    return 1.f;
  }
  virtual const LayerTreeSettings& Settings() const OVERRIDE {
    return settings_;
  }
  virtual void SetFullRootLayerDamage() OVERRIDE {}
  virtual bool HasImplThread() const OVERRIDE { return false; }
  virtual bool ShouldClearRootRenderPass() const OVERRIDE { return true; }
  virtual CompositorFrameMetadata MakeCompositorFrameMetadata() const
      OVERRIDE {
    return CompositorFrameMetadata();
  }
  virtual bool AllowPartialSwap() const OVERRIDE {
    return true;
  }
  virtual bool ExternalStencilTestEnabled() const OVERRIDE {
    return stencil_enabled_;
  }

  // OutputSurfaceClient implementation.
  virtual bool DeferredInitialize(
      scoped_refptr<ContextProvider> offscreen_context_provider) OVERRIDE {
    return false;
  }
  virtual void ReleaseGL() OVERRIDE {}
  virtual void SetNeedsRedrawRect(gfx::Rect damage_rect) OVERRIDE {}
  virtual void BeginFrame(const BeginFrameArgs& args) OVERRIDE {}
  virtual void OnSwapBuffersComplete(const CompositorFrameAck* ack) OVERRIDE {}
  virtual void DidLoseOutputSurface() OVERRIDE {}
  virtual void SetExternalDrawConstraints(const gfx::Transform& transform,
                                          gfx::Rect viewport) OVERRIDE {
    device_viewport_ = viewport;
  }
  virtual void SetExternalStencilTest(bool enable) OVERRIDE {
    stencil_enabled_ = enable;
  }
  virtual void SetMemoryPolicy(const ManagedMemoryPolicy& policy) OVERRIDE {}
  virtual void SetDiscardBackBufferWhenNotVisible(bool discard) OVERRIDE {}
  virtual void SetTreeActivationCallback(const base::Closure&) OVERRIDE {}

 private:
  gfx::Rect device_viewport_;
  bool stencil_enabled_;
  LayerTreeSettings settings_;
};

PixelTest::PixelTest()
    : device_viewport_size_(gfx::Size(200, 200)),
      fake_client_(
          new PixelTestRendererClient(gfx::Rect(device_viewport_size_))) {}

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

  renderer_->DecideRenderPassAllocationsForFrame(*pass_list);
  renderer_->DrawFrame(pass_list);

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

  // To rebaseline:
  // return WritePNGFile(*result_bitmap_, test_data_dir.Append(ref_file), true);

  return MatchesPNGFile(*result_bitmap_,
                        test_data_dir.Append(ref_file),
                        comparator);
}

void PixelTest::SetUpGLRenderer(bool use_skia_gpu_backend) {
  CHECK(fake_client_);
  CHECK(gfx::InitializeGLBindings(gfx::kGLImplementationOSMesaGL));

  using webkit::gpu::WebGraphicsContext3DInProcessCommandBufferImpl;
  scoped_ptr<WebKit::WebGraphicsContext3D> context3d(
      WebGraphicsContext3DInProcessCommandBufferImpl::CreateOffscreenContext(
          WebKit::WebGraphicsContext3D::Attributes()));
  output_surface_.reset(new PixelTestOutputSurface(
      context3d.PassAs<WebKit::WebGraphicsContext3D>()));
  output_surface_->BindToClient(fake_client_.get());

  resource_provider_ = ResourceProvider::Create(output_surface_.get(), 0);
  renderer_ = GLRenderer::Create(fake_client_.get(),
                                 output_surface_.get(),
                                 resource_provider_.get(),
                                 0,
                                 use_skia_gpu_backend).PassAs<DirectRenderer>();

  scoped_refptr<webkit::gpu::ContextProviderInProcess> offscreen_contexts =
      webkit::gpu::ContextProviderInProcess::CreateOffscreen();
  ASSERT_TRUE(offscreen_contexts->BindToCurrentThread());
  resource_provider_->set_offscreen_context_provider(offscreen_contexts);
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

void PixelTest::EnableExternalStencilTest() {
  fake_client_->SetExternalStencilTest(true);
}

void PixelTest::SetUpSoftwareRenderer() {
  CHECK(fake_client_);

  scoped_ptr<SoftwareOutputDevice> device(new PixelTestSoftwareOutputDevice());
  output_surface_.reset(new PixelTestOutputSurface(device.Pass()));
  output_surface_->BindToClient(fake_client_.get());
  resource_provider_ = ResourceProvider::Create(output_surface_.get(), 0);
  renderer_ = SoftwareRenderer::Create(
      fake_client_.get(),
      output_surface_.get(),
      resource_provider_.get()).PassAs<DirectRenderer>();
}

}  // namespace cc
