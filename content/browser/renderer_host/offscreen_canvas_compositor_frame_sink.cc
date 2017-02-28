// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/offscreen_canvas_compositor_frame_sink.h"

#include "base/memory/ptr_util.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_manager.h"
#include "content/browser/renderer_host/offscreen_canvas_compositor_frame_sink_provider_impl.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace content {

OffscreenCanvasCompositorFrameSink::OffscreenCanvasCompositorFrameSink(
    OffscreenCanvasCompositorFrameSinkProviderImpl* provider,
    const cc::FrameSinkId& frame_sink_id,
    cc::mojom::MojoCompositorFrameSinkRequest request,
    cc::mojom::MojoCompositorFrameSinkClientPtr client)
    : provider_(provider),
      support_(this,
               provider->GetSurfaceManager(),
               frame_sink_id,
               false /* is_root */,
               true /* handles_frame_sink_id_invalidation */,
               true /* needs_sync_points */),
      client_(std::move(client)),
      binding_(this, std::move(request)) {
  binding_.set_connection_error_handler(
      base::Bind(&OffscreenCanvasCompositorFrameSink::OnClientConnectionLost,
                 base::Unretained(this)));
}

OffscreenCanvasCompositorFrameSink::~OffscreenCanvasCompositorFrameSink() {
  provider_->OnCompositorFrameSinkClientDestroyed(support_.frame_sink_id());
}

void OffscreenCanvasCompositorFrameSink::SetNeedsBeginFrame(
    bool needs_begin_frame) {
  support_.SetNeedsBeginFrame(needs_begin_frame);
}

void OffscreenCanvasCompositorFrameSink::SubmitCompositorFrame(
    const cc::LocalSurfaceId& local_surface_id,
    cc::CompositorFrame frame) {
  // TODO(samans): This will need to do something similar to
  // GpuCompositorFrameSink.
  support_.SubmitCompositorFrame(local_surface_id, std::move(frame));
}

void OffscreenCanvasCompositorFrameSink::EvictFrame() {
  support_.EvictFrame();
}

void OffscreenCanvasCompositorFrameSink::DidReceiveCompositorFrameAck() {
  if (client_)
    client_->DidReceiveCompositorFrameAck();
}

void OffscreenCanvasCompositorFrameSink::OnBeginFrame(
    const cc::BeginFrameArgs& args) {
  if (client_)
    client_->OnBeginFrame(args);
}

void OffscreenCanvasCompositorFrameSink::ReclaimResources(
    const cc::ReturnedResourceArray& resources) {
  if (client_)
    client_->ReclaimResources(resources);
}

void OffscreenCanvasCompositorFrameSink::WillDrawSurface(
    const cc::LocalSurfaceId& local_surface_id,
    const gfx::Rect& damage_rect) {
  if (client_)
    client_->WillDrawSurface(local_surface_id, damage_rect);
}

void OffscreenCanvasCompositorFrameSink::OnClientConnectionLost() {
  provider_->OnCompositorFrameSinkClientConnectionLost(
      support_.frame_sink_id());
}

}  // namespace content
