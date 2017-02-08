// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/display_compositor/gpu_offscreen_compositor_frame_sink.h"

namespace display_compositor {

GpuOffscreenCompositorFrameSink::GpuOffscreenCompositorFrameSink(
    GpuCompositorFrameSinkDelegate* delegate,
    cc::SurfaceManager* surface_manager,
    const cc::FrameSinkId& frame_sink_id,
    cc::mojom::MojoCompositorFrameSinkRequest request,
    cc::mojom::MojoCompositorFrameSinkPrivateRequest
        compositor_frame_sink_private_request,
    cc::mojom::MojoCompositorFrameSinkClientPtr client)
    : GpuCompositorFrameSink(delegate,
                             surface_manager,
                             frame_sink_id,
                             nullptr,
                             nullptr,
                             std::move(compositor_frame_sink_private_request),
                             std::move(client)),
      binding_(this, std::move(request)) {
  binding_.set_connection_error_handler(
      base::Bind(&GpuOffscreenCompositorFrameSink::OnClientConnectionLost,
                 base::Unretained(this)));
}

GpuOffscreenCompositorFrameSink::~GpuOffscreenCompositorFrameSink() = default;

}  // namespace display_compositor
