// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/pixel_test_output_surface.h"

#include <utility>

#include "cc/output/compositor_frame.h"
#include "cc/output/output_surface_client.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/gfx/transform.h"

namespace cc {

PixelTestOutputSurface::PixelTestOutputSurface(
    scoped_refptr<ContextProvider> context_provider,
    scoped_refptr<ContextProvider> worker_context_provider,
    bool flipped_output_surface)
    : OutputSurface(std::move(context_provider),
                    std::move(worker_context_provider),
                    nullptr),
      external_stencil_test_(false) {
  capabilities_.adjust_deadline_for_parent = false;
  capabilities_.flipped_output_surface = flipped_output_surface;
}

PixelTestOutputSurface::PixelTestOutputSurface(
    scoped_refptr<ContextProvider> context_provider,
    bool flipped_output_surface)
    : PixelTestOutputSurface(std::move(context_provider),
                             nullptr,
                             flipped_output_surface) {}

PixelTestOutputSurface::PixelTestOutputSurface(
    std::unique_ptr<SoftwareOutputDevice> software_device)
    : OutputSurface(nullptr, nullptr, std::move(software_device)),
      external_stencil_test_(false) {}

PixelTestOutputSurface::~PixelTestOutputSurface() {}

void PixelTestOutputSurface::Reshape(const gfx::Size& size,
                                     float scale_factor,
                                     const gfx::ColorSpace& color_space,
                                     bool has_alpha) {
  gfx::Size expanded_size(size.width() + surface_expansion_size_.width(),
                          size.height() + surface_expansion_size_.height());
  OutputSurface::Reshape(expanded_size, scale_factor, color_space, has_alpha);
}

bool PixelTestOutputSurface::HasExternalStencilTest() const {
  return external_stencil_test_;
}

void PixelTestOutputSurface::SwapBuffers(CompositorFrame frame) {
  PostSwapBuffersComplete();
  client_->DidSwapBuffers();
}

uint32_t PixelTestOutputSurface::GetFramebufferCopyTextureFormat() {
  // This format will work if the |context_provider| has an RGB or RGBA
  // framebuffer. For now assume tests do not want/care about alpha in
  // the root render pass.
  return GL_RGB;
}

}  // namespace cc
