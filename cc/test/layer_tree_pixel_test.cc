// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/layer_tree_pixel_test.h"

#include "base/path_service.h"
#include "cc/test/paths.h"
#include "cc/test/pixel_test_utils.h"
#include "cc/trees/layer_tree_impl.h"
#include "ui/gl/gl_implementation.h"
#include "webkit/gpu/context_provider_in_process.h"
#include "webkit/gpu/webgraphicscontext3d_in_process_command_buffer_impl.h"

namespace cc {

LayerTreePixelTest::LayerTreePixelTest() {}

LayerTreePixelTest::~LayerTreePixelTest() {}

scoped_ptr<OutputSurface> LayerTreePixelTest::CreateOutputSurface() {
  CHECK(gfx::InitializeGLBindings(gfx::kGLImplementationOSMesaGL));

  using webkit::gpu::WebGraphicsContext3DInProcessCommandBufferImpl;
  scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl> context3d(
      new WebGraphicsContext3DInProcessCommandBufferImpl(
          WebKit::WebGraphicsContext3D::Attributes()));
  return make_scoped_ptr(
      new OutputSurface(context3d.PassAs<WebKit::WebGraphicsContext3D>()));
}

scoped_refptr<cc::ContextProvider>
LayerTreePixelTest::OffscreenContextProviderForMainThread() {
  scoped_refptr<webkit::gpu::ContextProviderInProcess> provider =
      webkit::gpu::ContextProviderInProcess::Create();
  CHECK(provider->BindToCurrentThread());
  return provider;
}

scoped_refptr<cc::ContextProvider>
LayerTreePixelTest::OffscreenContextProviderForCompositorThread() {
  scoped_refptr<webkit::gpu::ContextProviderInProcess> provider =
      webkit::gpu::ContextProviderInProcess::Create();
  CHECK(provider);
  return provider;
}

void LayerTreePixelTest::SwapBuffersOnThread(LayerTreeHostImpl* host_impl,
                                             bool result) {
  EXPECT_TRUE(result);

  gfx::Rect device_viewport_rect(
      host_impl->active_tree()->device_viewport_size());

  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config,
                   device_viewport_rect.width(),
                   device_viewport_rect.height());
  bitmap.allocPixels();
  unsigned char* pixels = static_cast<unsigned char*>(bitmap.getPixels());
  host_impl->Readback(pixels, device_viewport_rect);

  base::FilePath test_data_dir;
  EXPECT_TRUE(PathService::Get(cc::DIR_TEST_DATA, &test_data_dir));

  // To rebaseline:
  // EXPECT_TRUE(WritePNGFile(bitmap, test_data_dir.Append(ref_file_)));

  EXPECT_TRUE(MatchesPNGFile(bitmap, test_data_dir.Append(ref_file_),
                             ExactPixelComparator(true)));

  EndTest();
}

void LayerTreePixelTest::BeginTest() {
  PostSetNeedsCommitToMainThread();
}

void LayerTreePixelTest::AfterTest() {}

scoped_refptr<SolidColorLayer> LayerTreePixelTest::CreateSolidColorLayer(
    gfx::Rect rect, SkColor color) {
  scoped_refptr<SolidColorLayer> layer = SolidColorLayer::Create();
  layer->SetIsDrawable(true);
  layer->SetAnchorPoint(gfx::PointF());
  layer->SetBounds(rect.size());
  layer->SetPosition(rect.origin());
  layer->SetBackgroundColor(color);
  return layer;
}

void LayerTreePixelTest::RunPixelTest(
    scoped_refptr<Layer> content_root,
    base::FilePath file_name) {
  content_root_ = content_root;
  ref_file_ = file_name;
  RunTest(true);
}

void LayerTreePixelTest::SetupTree() {
  scoped_refptr<Layer> root = Layer::Create();
  root->SetBounds(content_root_->bounds());
  root->AddChild(content_root_);
  layer_tree_host()->SetRootLayer(root);
  LayerTreeTest::SetupTree();
}

}  // namespace cc
