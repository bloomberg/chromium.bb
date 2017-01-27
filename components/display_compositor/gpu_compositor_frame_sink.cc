// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/display_compositor/gpu_compositor_frame_sink.h"

#include "cc/surfaces/surface_reference.h"

namespace display_compositor {

GpuCompositorFrameSink::GpuCompositorFrameSink(
    GpuCompositorFrameSinkDelegate* delegate,
    cc::SurfaceManager* surface_manager,
    const cc::FrameSinkId& frame_sink_id,
    std::unique_ptr<cc::Display> display,
    std::unique_ptr<cc::BeginFrameSource> begin_frame_source,
    cc::mojom::MojoCompositorFrameSinkPrivateRequest
        compositor_frame_sink_private_request,
    cc::mojom::MojoCompositorFrameSinkClientPtr client)
    : delegate_(delegate),
      support_(this,
               surface_manager,
               frame_sink_id,
               std::move(display),
               std::move(begin_frame_source)),
      surface_manager_(surface_manager),
      surface_tracker_(frame_sink_id),
      client_(std::move(client)),
      compositor_frame_sink_private_binding_(
          this,
          std::move(compositor_frame_sink_private_request)) {
  compositor_frame_sink_private_binding_.set_connection_error_handler(
      base::Bind(&GpuCompositorFrameSink::OnPrivateConnectionLost,
                 base::Unretained(this)));
}

GpuCompositorFrameSink::~GpuCompositorFrameSink() {
  // For display root surfaces, remove the reference from top level root to
  // indicate the display root surface is no longer visible.
  if (support_.display() && surface_tracker_.current_surface_id().is_valid()) {
    const cc::SurfaceId top_level_root_surface_id =
        surface_manager_->GetRootSurfaceId();
    std::vector<cc::SurfaceReference> references_to_remove{cc::SurfaceReference(
        top_level_root_surface_id, surface_tracker_.current_surface_id())};
    surface_manager_->RemoveSurfaceReferences(references_to_remove);
  }
}

void GpuCompositorFrameSink::EvictFrame() {
  support_.EvictFrame();
}

void GpuCompositorFrameSink::SetNeedsBeginFrame(bool needs_begin_frame) {
  support_.SetNeedsBeginFrame(needs_begin_frame);
}

void GpuCompositorFrameSink::SubmitCompositorFrame(
    const cc::LocalFrameId& local_frame_id,
    cc::CompositorFrame frame) {
  cc::SurfaceId start_surface_id = surface_tracker_.current_surface_id();
  surface_tracker_.UpdateReferences(local_frame_id,
                                    frame.metadata.referenced_surfaces);
  // TODO(kylechar): Move adding top-level root references to
  // GpuDisplayCompositorFrameSink.

  support_.SubmitCompositorFrame(local_frame_id, std::move(frame));

  // Get the list of surfaces to add/remove from |surface_tracker_| so we can
  // append to them before adding/removing.
  std::vector<cc::SurfaceReference>& references_to_add =
      surface_tracker_.references_to_add();
  std::vector<cc::SurfaceReference>& references_to_remove =
      surface_tracker_.references_to_remove();

  // Append TLR references for the display root surfaces when display root
  // surface changes.
  if (support_.display() &&
      start_surface_id != surface_tracker_.current_surface_id()) {
    const cc::SurfaceId top_level_root_surface_id =
        surface_manager_->GetRootSurfaceId();

    // The first frame will not have a valid |start_surface_id| and there will
    // be no surface to remove.
    if (start_surface_id.local_frame_id().is_valid()) {
      references_to_remove.push_back(
          cc::SurfaceReference(top_level_root_surface_id, start_surface_id));
    }

    references_to_add.push_back(cc::SurfaceReference(
        top_level_root_surface_id, surface_tracker_.current_surface_id()));
  }

  if (!references_to_add.empty())
    surface_manager_->AddSurfaceReferences(references_to_add);
  if (!references_to_remove.empty())
    surface_manager_->RemoveSurfaceReferences(references_to_remove);
}

void GpuCompositorFrameSink::Require(const cc::LocalFrameId& local_frame_id,
                                     const cc::SurfaceSequence& sequence) {
  support_.Require(local_frame_id, sequence);
}

void GpuCompositorFrameSink::Satisfy(const cc::SurfaceSequence& sequence) {
  support_.Satisfy(sequence);
}

void GpuCompositorFrameSink::DidReceiveCompositorFrameAck() {
  if (client_)
    client_->DidReceiveCompositorFrameAck();
}

void GpuCompositorFrameSink::AddChildFrameSink(
    const cc::FrameSinkId& child_frame_sink_id) {
  support_.AddChildFrameSink(child_frame_sink_id);
}

void GpuCompositorFrameSink::RemoveChildFrameSink(
    const cc::FrameSinkId& child_frame_sink_id) {
  support_.RemoveChildFrameSink(child_frame_sink_id);
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

void GpuCompositorFrameSink::WillDrawSurface() {
  if (client_)
    client_->WillDrawSurface();
}

void GpuCompositorFrameSink::OnClientConnectionLost() {
  client_connection_lost_ = true;
  // Request destruction of |this| only if both connections are lost.
  delegate_->OnClientConnectionLost(support_.frame_sink_id(),
                                    private_connection_lost_);
}

void GpuCompositorFrameSink::OnPrivateConnectionLost() {
  private_connection_lost_ = true;
  // Request destruction of |this| only if both connections are lost.
  delegate_->OnPrivateConnectionLost(support_.frame_sink_id(),
                                     client_connection_lost_);
}

}  // namespace display_compositor
