// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/pixel_test.h"

#include "base/path_service.h"
#include "base/run_loop.h"
#include "cc/output/compositor_frame_metadata.h"
#include "cc/output/gl_renderer.h"
#include "cc/output/output_surface.h"
#include "cc/output/software_renderer.h"
#include "cc/resources/resource_provider.h"
#include "cc/test/paths.h"
#include "cc/test/pixel_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_implementation.h"
#include "webkit/gpu/context_provider_in_process.h"
#include "webkit/gpu/webgraphicscontext3d_in_process_command_buffer_impl.h"

namespace cc {

class PixelTest::PixelTestRendererClient : public RendererClient {
 public:
  explicit PixelTestRendererClient(gfx::Size device_viewport_size)
      : device_viewport_size_(device_viewport_size) {}

  // RendererClient implementation.
  virtual gfx::Size DeviceViewportSize() const OVERRIDE {
    return device_viewport_size_;
  }
  virtual const LayerTreeSettings& Settings() const OVERRIDE {
    return settings_;
  }
  virtual void SetFullRootLayerDamage() OVERRIDE {}
  virtual void SetManagedMemoryPolicy(
      const ManagedMemoryPolicy& policy) OVERRIDE {}
  virtual void EnforceManagedMemoryPolicy(
      const ManagedMemoryPolicy& policy) OVERRIDE {}
  virtual bool HasImplThread() const OVERRIDE { return false; }
  virtual bool ShouldClearRootRenderPass() const OVERRIDE { return true; }
  virtual CompositorFrameMetadata MakeCompositorFrameMetadata() const
      OVERRIDE {
    return CompositorFrameMetadata();
  }
  virtual bool AllowPartialSwap() const OVERRIDE {
    return true;
  }

 private:
  gfx::Size device_viewport_size_;
  LayerTreeSettings settings_;
};

PixelTest::PixelTest()
    : device_viewport_size_(gfx::Size(200, 200)),
      fake_client_(new PixelTestRendererClient(device_viewport_size_)) {}

PixelTest::~PixelTest() {}

bool PixelTest::RunPixelTest(RenderPassList* pass_list,
                             const base::FilePath& ref_file,
                             const PixelComparator& comparator) {
  base::RunLoop run_loop;

  pass_list->back()->copy_callbacks.push_back(
      base::Bind(&PixelTest::ReadbackResult,
                 base::Unretained(this),
                 run_loop.QuitClosure()));

  renderer_->DecideRenderPassAllocationsForFrame(*pass_list);
  renderer_->DrawFrame(pass_list);

  // Wait for the readback to complete.
  resource_provider_->Finish();
  run_loop.Run();

  return PixelsMatchReference(ref_file, comparator);
}

void PixelTest::ReadbackResult(base::Closure quit_run_loop,
                               scoped_ptr<SkBitmap> bitmap) {
  result_bitmap_ = bitmap.Pass();
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

void PixelTest::SetUpGLRenderer() {
  CHECK(fake_client_);
  CHECK(gfx::InitializeGLBindings(gfx::kGLImplementationOSMesaGL));

  using webkit::gpu::WebGraphicsContext3DInProcessCommandBufferImpl;
  scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl> context3d(
      WebGraphicsContext3DInProcessCommandBufferImpl::CreateOffscreenContext(
          WebKit::WebGraphicsContext3D::Attributes()));
  output_surface_.reset(new OutputSurface(
      context3d.PassAs<WebKit::WebGraphicsContext3D>()));
  resource_provider_ = ResourceProvider::Create(output_surface_.get(), 0);
  renderer_ = GLRenderer::Create(fake_client_.get(),
                                 output_surface_.get(),
                                 resource_provider_.get(),
                                 0).PassAs<DirectRenderer>();

  scoped_refptr<webkit::gpu::ContextProviderInProcess> offscreen_contexts =
      webkit::gpu::ContextProviderInProcess::Create();
  ASSERT_TRUE(offscreen_contexts->BindToCurrentThread());
  resource_provider_->set_offscreen_context_provider(offscreen_contexts);
}

void PixelTest::SetUpSoftwareRenderer() {
  CHECK(fake_client_);

  scoped_ptr<SoftwareOutputDevice> device(new SoftwareOutputDevice());
  output_surface_.reset(new OutputSurface(device.Pass()));
  resource_provider_ = ResourceProvider::Create(output_surface_.get(), 0);
  renderer_ = SoftwareRenderer::Create(
      fake_client_.get(),
      output_surface_.get(),
      resource_provider_.get()).PassAs<DirectRenderer>();
}

}  // namespace cc
