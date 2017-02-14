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
                             base::MakeUnique<cc::CompositorFrameSinkSupport>(
                                 this,
                                 surface_manager,
                                 frame_sink_id,
                                 true /* is_root */,
                                 true /* handles_frame_sink_id_invalidation */,
                                 true /* needs_sync_points */),
                             std::move(compositor_frame_sink_private_request),
                             std::move(client)),
      binding_(this, std::move(request)),
      display_private_binding_(this, std::move(display_private_request)),
      display_begin_frame_source_(std::move(begin_frame_source)),
      display_(std::move(display)) {
  binding_.set_connection_error_handler(
      base::Bind(&GpuDisplayCompositorFrameSink::OnClientConnectionLost,
                 base::Unretained(this)));
  display_->Initialize(this, surface_manager);
  display_->SetVisible(true);
}

GpuDisplayCompositorFrameSink::~GpuDisplayCompositorFrameSink() = default;

void GpuDisplayCompositorFrameSink::SetDisplayVisible(bool visible) {
  DCHECK(display_);
  display_->SetVisible(visible);
}

void GpuDisplayCompositorFrameSink::ResizeDisplay(const gfx::Size& size) {
  DCHECK(display_);
  display_->Resize(size);
}

void GpuDisplayCompositorFrameSink::SetDisplayColorSpace(
    const gfx::ColorSpace& color_space) {
  DCHECK(display_);
  display_->SetColorSpace(color_space);
}

void GpuDisplayCompositorFrameSink::SetOutputIsSecure(bool secure) {
  DCHECK(display_);
  display_->SetOutputIsSecure(secure);
}

void GpuDisplayCompositorFrameSink::SetLocalSurfaceId(
    const cc::LocalSurfaceId& local_surface_id,
    float scale_factor) {
  display_->SetLocalSurfaceId(local_surface_id, scale_factor);
}

void GpuDisplayCompositorFrameSink::DisplayOutputSurfaceLost() {
  // TODO(staraz): Implement this. Client should hear about context/output
  // surface lost.
}

void GpuDisplayCompositorFrameSink::DisplayWillDrawAndSwap(
    bool will_draw_and_swap,
    const cc::RenderPassList& render_pass) {}

void GpuDisplayCompositorFrameSink::DisplayDidDrawAndSwap() {}

}  // namespace display_compositor
