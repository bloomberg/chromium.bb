// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/display_compositor/gpu_display_compositor_frame_sink.h"

namespace display_compositor {

GpuDisplayCompositorFrameSink::GpuDisplayCompositorFrameSink(
    GpuCompositorFrameSinkDelegate* delegate,
    cc::SurfaceManager* surface_manager,
    const cc::FrameSinkId& frame_sink_id,
    std::unique_ptr<cc::Display> display,
    std::unique_ptr<cc::BeginFrameSource> begin_frame_source,
    cc::mojom::MojoCompositorFrameSinkAssociatedRequest request,
    cc::mojom::MojoCompositorFrameSinkPrivateRequest
        compositor_frame_sink_private_request,
    cc::mojom::MojoCompositorFrameSinkClientPtr client,
    cc::mojom::DisplayPrivateAssociatedRequest display_private_request)
    : GpuCompositorFrameSink(delegate,
                             surface_manager,
                             frame_sink_id,
                             std::move(display),
                             std::move(begin_frame_source),
                             std::move(compositor_frame_sink_private_request),
                             std::move(client)),
      binding_(this, std::move(request)),
      display_private_binding_(this, std::move(display_private_request)) {
  binding_.set_connection_error_handler(
      base::Bind(&GpuDisplayCompositorFrameSink::OnClientConnectionLost,
                 base::Unretained(this)));
}

GpuDisplayCompositorFrameSink::~GpuDisplayCompositorFrameSink() = default;

void GpuDisplayCompositorFrameSink::SetDisplayVisible(bool visible) {
  DCHECK(support_.display());
  support_.display()->SetVisible(visible);
}

void GpuDisplayCompositorFrameSink::ResizeDisplay(const gfx::Size& size) {
  DCHECK(support_.display());
  support_.display()->Resize(size);
}

void GpuDisplayCompositorFrameSink::SetDisplayColorSpace(
    const gfx::ColorSpace& color_space) {
  DCHECK(support_.display());
  support_.display()->SetColorSpace(color_space);
}

void GpuDisplayCompositorFrameSink::SetOutputIsSecure(bool secure) {
  DCHECK(support_.display());
  support_.display()->SetOutputIsSecure(secure);
}

}  // namespace display_compositor
