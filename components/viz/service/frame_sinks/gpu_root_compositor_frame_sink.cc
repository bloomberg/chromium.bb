// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/gpu_root_compositor_frame_sink.h"

#include <utility>

#include "cc/surfaces/compositor_frame_sink_support.h"
#include "cc/surfaces/display.h"

namespace viz {

GpuRootCompositorFrameSink::GpuRootCompositorFrameSink(
    GpuCompositorFrameSinkDelegate* delegate,
    cc::SurfaceManager* surface_manager,
    const cc::FrameSinkId& frame_sink_id,
    std::unique_ptr<cc::Display> display,
    std::unique_ptr<cc::BeginFrameSource> begin_frame_source,
    cc::mojom::CompositorFrameSinkAssociatedRequest request,
    cc::mojom::CompositorFrameSinkPrivateRequest
        compositor_frame_sink_private_request,
    cc::mojom::CompositorFrameSinkClientPtr client,
    cc::mojom::DisplayPrivateAssociatedRequest display_private_request)
    : delegate_(delegate),
      support_(cc::CompositorFrameSinkSupport::Create(
          this,
          surface_manager,
          frame_sink_id,
          true /* is_root */,
          true /* handles_frame_sink_id_invalidation */,
          true /* needs_sync_points */)),
      display_begin_frame_source_(std::move(begin_frame_source)),
      display_(std::move(display)),
      client_(std::move(client)),
      compositor_frame_sink_binding_(this, std::move(request)),
      compositor_frame_sink_private_binding_(
          this,
          std::move(compositor_frame_sink_private_request)),
      display_private_binding_(this, std::move(display_private_request)) {
  DCHECK(display_begin_frame_source_);
  compositor_frame_sink_binding_.set_connection_error_handler(
      base::Bind(&GpuRootCompositorFrameSink::OnClientConnectionLost,
                 base::Unretained(this)));
  compositor_frame_sink_private_binding_.set_connection_error_handler(
      base::Bind(&GpuRootCompositorFrameSink::OnPrivateConnectionLost,
                 base::Unretained(this)));
  surface_manager->RegisterBeginFrameSource(display_begin_frame_source_.get(),
                                            frame_sink_id);
  display_->Initialize(this, surface_manager);
}

GpuRootCompositorFrameSink::~GpuRootCompositorFrameSink() {
  support_->surface_manager()->UnregisterBeginFrameSource(
      display_begin_frame_source_.get());
}

void GpuRootCompositorFrameSink::SetDisplayVisible(bool visible) {
  DCHECK(display_);
  display_->SetVisible(visible);
}

void GpuRootCompositorFrameSink::ResizeDisplay(const gfx::Size& size) {
  DCHECK(display_);
  display_->Resize(size);
}

void GpuRootCompositorFrameSink::SetDisplayColorSpace(
    const gfx::ColorSpace& color_space) {
  DCHECK(display_);
  display_->SetColorSpace(color_space, color_space);
}

void GpuRootCompositorFrameSink::SetOutputIsSecure(bool secure) {
  DCHECK(display_);
  display_->SetOutputIsSecure(secure);
}

void GpuRootCompositorFrameSink::SetLocalSurfaceId(
    const cc::LocalSurfaceId& local_surface_id,
    float scale_factor) {
  display_->SetLocalSurfaceId(local_surface_id, scale_factor);
}

void GpuRootCompositorFrameSink::SetNeedsBeginFrame(bool needs_begin_frame) {
  support_->SetNeedsBeginFrame(needs_begin_frame);
}

void GpuRootCompositorFrameSink::SubmitCompositorFrame(
    const cc::LocalSurfaceId& local_surface_id,
    cc::CompositorFrame frame) {
  if (!support_->SubmitCompositorFrame(local_surface_id, std::move(frame))) {
    compositor_frame_sink_binding_.Close();
    OnClientConnectionLost();
  }
}

void GpuRootCompositorFrameSink::DidNotProduceFrame(
    const cc::BeginFrameAck& begin_frame_ack) {
  support_->DidNotProduceFrame(begin_frame_ack);
}

void GpuRootCompositorFrameSink::ClaimTemporaryReference(
    const cc::SurfaceId& surface_id) {
  support_->ClaimTemporaryReference(surface_id);
}

void GpuRootCompositorFrameSink::RequestCopyOfSurface(
    std::unique_ptr<cc::CopyOutputRequest> request) {
  support_->RequestCopyOfSurface(std::move(request));
}

void GpuRootCompositorFrameSink::DisplayOutputSurfaceLost() {
  // TODO(staraz): Implement this. Client should hear about context/output
  // surface lost.
}

void GpuRootCompositorFrameSink::DisplayWillDrawAndSwap(
    bool will_draw_and_swap,
    const cc::RenderPassList& render_pass) {}

void GpuRootCompositorFrameSink::DisplayDidDrawAndSwap() {}

void GpuRootCompositorFrameSink::DidReceiveCompositorFrameAck(
    const std::vector<cc::ReturnedResource>& resources) {
  if (client_)
    client_->DidReceiveCompositorFrameAck(resources);
}

void GpuRootCompositorFrameSink::OnBeginFrame(const cc::BeginFrameArgs& args) {
  if (client_)
    client_->OnBeginFrame(args);
}

void GpuRootCompositorFrameSink::ReclaimResources(
    const std::vector<cc::ReturnedResource>& resources) {
  if (client_)
    client_->ReclaimResources(resources);
}

void GpuRootCompositorFrameSink::WillDrawSurface(
    const cc::LocalSurfaceId& local_surface_id,
    const gfx::Rect& damage_rect) {}

void GpuRootCompositorFrameSink::OnClientConnectionLost() {
  delegate_->OnClientConnectionLost(support_->frame_sink_id());
}

void GpuRootCompositorFrameSink::OnPrivateConnectionLost() {
  delegate_->OnPrivateConnectionLost(support_->frame_sink_id());
}

}  // namespace viz
