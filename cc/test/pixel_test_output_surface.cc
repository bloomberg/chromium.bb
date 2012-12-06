// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/pixel_test_output_surface.h"

#include "webkit/gpu/webgraphicscontext3d_in_process_command_buffer_impl.h"

namespace cc {

PixelTestOutputSurface::PixelTestOutputSurface() {
  scoped_ptr<webkit::gpu::WebGraphicsContext3DInProcessCommandBufferImpl>
      context(new webkit::gpu::WebGraphicsContext3DInProcessCommandBufferImpl);
  context->Initialize(WebKit::WebGraphicsContext3D::Attributes(), NULL);
  context_ = context.PassAs<WebKit::WebGraphicsContext3D>();
}

PixelTestOutputSurface::~PixelTestOutputSurface() {
}

bool PixelTestOutputSurface::bindToClient(
    WebKit::WebCompositorOutputSurfaceClient*) {
  return context_->makeContextCurrent();
}

const WebKit::WebCompositorOutputSurface::Capabilities&
    PixelTestOutputSurface::capabilities() const {
  static WebKit::WebCompositorOutputSurface::Capabilities capabilities;
  return capabilities;
}

WebKit::WebGraphicsContext3D* PixelTestOutputSurface::context3D() const {
  return context_.get();
}

}  // namespace cc
