// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/pixel_test_output_surface.h"

#include "cc/output/output_surface_client.h"
#include "cc/scheduler/begin_frame_source.h"
#include "ui/gfx/transform.h"

namespace cc {

PixelTestOutputSurface::PixelTestOutputSurface(
    scoped_refptr<ContextProvider> context_provider,
    scoped_refptr<ContextProvider> worker_context_provider,
    bool flipped_output_surface,
    std::unique_ptr<BeginFrameSource> begin_frame_source)
    : OutputSurface(context_provider, worker_context_provider),
      begin_frame_source_(std::move(begin_frame_source)),
      external_stencil_test_(false) {
  capabilities_.adjust_deadline_for_parent = false;
  capabilities_.flipped_output_surface = flipped_output_surface;
}

PixelTestOutputSurface::PixelTestOutputSurface(
    scoped_refptr<ContextProvider> context_provider,
    bool flipped_output_surface,
    std::unique_ptr<BeginFrameSource> begin_frame_source)
    : PixelTestOutputSurface(context_provider,
                             nullptr,
                             flipped_output_surface,
                             std::move(begin_frame_source)) {}

PixelTestOutputSurface::PixelTestOutputSurface(
    std::unique_ptr<SoftwareOutputDevice> software_device,
    std::unique_ptr<BeginFrameSource> begin_frame_source)
    : OutputSurface(std::move(software_device)),
      begin_frame_source_(std::move(begin_frame_source)),
      external_stencil_test_(false) {}

PixelTestOutputSurface::~PixelTestOutputSurface() {}

bool PixelTestOutputSurface::BindToClient(OutputSurfaceClient* client) {
  if (!OutputSurface::BindToClient(client))
    return false;

  // TODO(enne): Once the renderer uses begin frame sources, this will
  // always be valid.
  if (begin_frame_source_)
    client->SetBeginFrameSource(begin_frame_source_.get());
  return true;
}

void PixelTestOutputSurface::Reshape(const gfx::Size& size,
                                     float scale_factor,
                                     bool has_alpha) {
  gfx::Size expanded_size(size.width() + surface_expansion_size_.width(),
                          size.height() + surface_expansion_size_.height());
  OutputSurface::Reshape(expanded_size, scale_factor, has_alpha);
}

bool PixelTestOutputSurface::HasExternalStencilTest() const {
  return external_stencil_test_;
}

void PixelTestOutputSurface::SwapBuffers(CompositorFrame* frame) {
  PostSwapBuffersComplete();
  client_->DidSwapBuffers();
}

}  // namespace cc
