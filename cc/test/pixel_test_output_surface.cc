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

bool PixelTestOutputSurface::BindToClient(OutputSurfaceClient*) {
  return context_->makeContextCurrent();
}

const struct OutputSurface::Capabilities& PixelTestOutputSurface::Capabilities()
    const {
  static struct OutputSurface::Capabilities capabilities;
  return capabilities;
}

WebKit::WebGraphicsContext3D* PixelTestOutputSurface::Context3D() const {
  return context_.get();
}

SoftwareOutputDevice* PixelTestOutputSurface::SoftwareDevice() const {
  return NULL;
}

}  // namespace cc
