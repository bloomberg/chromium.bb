// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/display_compositor/gpu_compositor_frame_sink.h"

#include "cc/surfaces/surface_reference.h"

namespace display_compositor {

GpuCompositorFrameSink::GpuCompositorFrameSink(
    GpuCompositorFrameSinkDelegate* delegate,
    std::unique_ptr<cc::CompositorFrameSinkSupport> support,
    cc::mojom::MojoCompositorFrameSinkPrivateRequest
        compositor_frame_sink_private_request,
    cc::mojom::MojoCompositorFrameSinkClientPtr client)
    : delegate_(delegate),
      support_(std::move(support)),
      client_(std::move(client)),
      compositor_frame_sink_private_binding_(
          this,
          std::move(compositor_frame_sink_private_request)) {
  compositor_frame_sink_private_binding_.set_connection_error_handler(
      base::Bind(&GpuCompositorFrameSink::OnPrivateConnectionLost,
                 base::Unretained(this)));
}

GpuCompositorFrameSink::~GpuCompositorFrameSink() {}

void GpuCompositorFrameSink::EvictFrame() {
  support_->EvictFrame();
}

void GpuCompositorFrameSink::SetNeedsBeginFrame(bool needs_begin_frame) {
  support_->SetNeedsBeginFrame(needs_begin_frame);
}

void GpuCompositorFrameSink::SubmitCompositorFrame(
    const cc::LocalSurfaceId& local_surface_id,
    cc::CompositorFrame frame) {
  support_->SubmitCompositorFrame(local_surface_id, std::move(frame));
}

void GpuCompositorFrameSink::Require(const cc::LocalSurfaceId& local_surface_id,
                                     const cc::SurfaceSequence& sequence) {
  support_->Require(local_surface_id, sequence);
}

void GpuCompositorFrameSink::Satisfy(const cc::SurfaceSequence& sequence) {
  support_->Satisfy(sequence);
}

void GpuCompositorFrameSink::DidReceiveCompositorFrameAck() {
  if (client_)
    client_->DidReceiveCompositorFrameAck();
}

void GpuCompositorFrameSink::AddChildFrameSink(
    const cc::FrameSinkId& child_frame_sink_id) {
  support_->AddChildFrameSink(child_frame_sink_id);
}

void GpuCompositorFrameSink::RemoveChildFrameSink(
    const cc::FrameSinkId& child_frame_sink_id) {
  support_->RemoveChildFrameSink(child_frame_sink_id);
}

void GpuCompositorFrameSink::OnBeginFrame(const cc::BeginFrameArgs& args) {
  if (client_)
    client_->OnBeginFrame(args);
}

void GpuCompositorFrameSink::ReclaimResources(
    const cc::ReturnedResourceArray& resources) {
  if (client_)
    client_->ReclaimResources(resources);
}

void GpuCompositorFrameSink::WillDrawSurface(
    const cc::LocalSurfaceId& local_surface_id,
    const gfx::Rect& damage_rect) {
  if (client_)
    client_->WillDrawSurface(local_surface_id, damage_rect);
}

void GpuCompositorFrameSink::OnClientConnectionLost() {
  client_connection_lost_ = true;
  // Request destruction of |this| only if both connections are lost.
  delegate_->OnClientConnectionLost(support_->frame_sink_id(),
                                    private_connection_lost_);
}

void GpuCompositorFrameSink::OnPrivateConnectionLost() {
  private_connection_lost_ = true;
  // Request destruction of |this| only if both connections are lost.
  delegate_->OnPrivateConnectionLost(support_->frame_sink_id(),
                                     client_connection_lost_);
}

}  // namespace display_compositor
