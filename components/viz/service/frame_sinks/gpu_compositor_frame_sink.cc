// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/gpu_compositor_frame_sink.h"

#include <utility>

#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"

namespace viz {

GpuCompositorFrameSink::GpuCompositorFrameSink(
    FrameSinkManagerImpl* frame_sink_manager,
    const FrameSinkId& frame_sink_id,
    cc::mojom::CompositorFrameSinkRequest request,
    cc::mojom::CompositorFrameSinkPrivateRequest
        compositor_frame_sink_private_request,
    cc::mojom::CompositorFrameSinkClientPtr client)
    : support_(CompositorFrameSinkSupport::Create(
          this,
          frame_sink_manager,
          frame_sink_id,
          false /* is_root */,
          true /* handles_frame_sink_id_invalidation */,
          true /* needs_sync_points */)),
      client_(std::move(client)),
      compositor_frame_sink_binding_(this, std::move(request)),
      compositor_frame_sink_private_binding_(
          this,
          std::move(compositor_frame_sink_private_request)) {
  compositor_frame_sink_binding_.set_connection_error_handler(base::Bind(
      &GpuCompositorFrameSink::OnClientConnectionLost, base::Unretained(this)));
  compositor_frame_sink_private_binding_.set_connection_error_handler(
      base::Bind(&GpuCompositorFrameSink::OnPrivateConnectionLost,
                 base::Unretained(this)));
}

GpuCompositorFrameSink::~GpuCompositorFrameSink() = default;

void GpuCompositorFrameSink::SetNeedsBeginFrame(bool needs_begin_frame) {
  support_->SetNeedsBeginFrame(needs_begin_frame);
}

void GpuCompositorFrameSink::SubmitCompositorFrame(
    const LocalSurfaceId& local_surface_id,
    cc::CompositorFrame frame) {
  if (!support_->SubmitCompositorFrame(local_surface_id, std::move(frame))) {
    compositor_frame_sink_binding_.CloseWithReason(
        1, "Surface invariants violation");
    OnClientConnectionLost();
  }
}

void GpuCompositorFrameSink::DidNotProduceFrame(
    const cc::BeginFrameAck& begin_frame_ack) {
  support_->DidNotProduceFrame(begin_frame_ack);
}

void GpuCompositorFrameSink::DidReceiveCompositorFrameAck(
    const std::vector<cc::ReturnedResource>& resources) {
  if (client_)
    client_->DidReceiveCompositorFrameAck(resources);
}

void GpuCompositorFrameSink::ClaimTemporaryReference(
    const SurfaceId& surface_id) {
  support_->ClaimTemporaryReference(surface_id);
}

void GpuCompositorFrameSink::RequestCopyOfSurface(
    std::unique_ptr<cc::CopyOutputRequest> request) {
  support_->RequestCopyOfSurface(std::move(request));
}

void GpuCompositorFrameSink::OnBeginFrame(const cc::BeginFrameArgs& args) {
  if (client_)
    client_->OnBeginFrame(args);
}

void GpuCompositorFrameSink::OnBeginFramePausedChanged(bool paused) {
  if (client_)
    client_->OnBeginFramePausedChanged(paused);
}

void GpuCompositorFrameSink::ReclaimResources(
    const std::vector<cc::ReturnedResource>& resources) {
  if (client_)
    client_->ReclaimResources(resources);
}

void GpuCompositorFrameSink::WillDrawSurface(
    const LocalSurfaceId& local_surface_id,
    const gfx::Rect& damage_rect) {}

void GpuCompositorFrameSink::OnClientConnectionLost() {
  support_->frame_sink_manager()->OnClientConnectionLost(
      support_->frame_sink_id());
}

void GpuCompositorFrameSink::OnPrivateConnectionLost() {
  support_->frame_sink_manager()->OnPrivateConnectionLost(
      support_->frame_sink_id());
}

}  // namespace viz
