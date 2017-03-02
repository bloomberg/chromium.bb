// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/pixel_test_output_surface.h"

#include <utility>

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/output/output_surface_client.h"
#include "cc/output/output_surface_frame.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/gfx/transform.h"

namespace cc {

PixelTestOutputSurface::PixelTestOutputSurface(
    scoped_refptr<ContextProvider> context_provider,
    bool flipped_output_surface)
    : OutputSurface(std::move(context_provider)), weak_ptr_factory_(this) {
  capabilities_.flipped_output_surface = flipped_output_surface;
  capabilities_.supports_stencil = true;
}

PixelTestOutputSurface::PixelTestOutputSurface(
    std::unique_ptr<SoftwareOutputDevice> software_device)
    : OutputSurface(std::move(software_device)), weak_ptr_factory_(this) {
  capabilities_.supports_stencil = true;
}

PixelTestOutputSurface::~PixelTestOutputSurface() = default;

void PixelTestOutputSurface::BindToClient(OutputSurfaceClient* client) {
  client_ = client;
}

void PixelTestOutputSurface::EnsureBackbuffer() {}

void PixelTestOutputSurface::DiscardBackbuffer() {}

void PixelTestOutputSurface::BindFramebuffer() {
  context_provider()->ContextGL()->BindFramebuffer(GL_FRAMEBUFFER, 0);
}

void PixelTestOutputSurface::SetDrawRectangle(const gfx::Rect& rect) {}

void PixelTestOutputSurface::Reshape(const gfx::Size& size,
                                     float device_scale_factor,
                                     const gfx::ColorSpace& color_space,
                                     bool has_alpha,
                                     bool use_stencil) {
  // External stencil test cannot be tested at the same time as |use_stencil|.
  DCHECK(!use_stencil || !external_stencil_test_);
  if (context_provider()) {
    context_provider()->ContextGL()->ResizeCHROMIUM(
        size.width(), size.height(), device_scale_factor, has_alpha);
  } else {
    software_device()->Resize(size, device_scale_factor);
  }
}

bool PixelTestOutputSurface::HasExternalStencilTest() const {
  return external_stencil_test_;
}

void PixelTestOutputSurface::ApplyExternalStencil() {}

void PixelTestOutputSurface::SwapBuffers(OutputSurfaceFrame frame) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&PixelTestOutputSurface::SwapBuffersCallback,
                            weak_ptr_factory_.GetWeakPtr()));
}

void PixelTestOutputSurface::SwapBuffersCallback() {
  client_->DidReceiveSwapBuffersAck();
}

OverlayCandidateValidator*
PixelTestOutputSurface::GetOverlayCandidateValidator() const {
  return nullptr;
}

bool PixelTestOutputSurface::IsDisplayedAsOverlayPlane() const {
  return false;
}

unsigned PixelTestOutputSurface::GetOverlayTextureId() const {
  return 0;
}

bool PixelTestOutputSurface::SurfaceIsSuspendForRecycle() const {
  return false;
}

uint32_t PixelTestOutputSurface::GetFramebufferCopyTextureFormat() {
  // This format will work if the |context_provider| has an RGB or RGBA
  // framebuffer. For now assume tests do not want/care about alpha in
  // the root render pass.
  return GL_RGB;
}

}  // namespace cc
