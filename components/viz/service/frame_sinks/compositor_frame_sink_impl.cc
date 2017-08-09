// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/compositor_frame_sink_impl.h"

#include <utility>

#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"

namespace viz {

CompositorFrameSinkImpl::CompositorFrameSinkImpl(
    FrameSinkManagerImpl* frame_sink_manager,
    const FrameSinkId& frame_sink_id,
    mojom::CompositorFrameSinkRequest request,
    mojom::CompositorFrameSinkClientPtr client)
    : support_(
          CompositorFrameSinkSupport::Create(this,
                                             frame_sink_manager,
                                             frame_sink_id,
                                             false /* is_root */,
                                             true /* needs_sync_points */)),
      client_(std::move(client)),
      compositor_frame_sink_binding_(this, std::move(request)) {
  compositor_frame_sink_binding_.set_connection_error_handler(
      base::Bind(&CompositorFrameSinkImpl::OnClientConnectionLost,
                 base::Unretained(this)));
}

CompositorFrameSinkImpl::~CompositorFrameSinkImpl() = default;

void CompositorFrameSinkImpl::SetNeedsBeginFrame(bool needs_begin_frame) {
  support_->SetNeedsBeginFrame(needs_begin_frame);
}

void CompositorFrameSinkImpl::SubmitCompositorFrame(
    const LocalSurfaceId& local_surface_id,
    cc::CompositorFrame frame,
    mojom::HitTestRegionListPtr hit_test_region_list,
    uint64_t submit_time) {
  // TODO(gklassen): Route hit-test data to the appropriate HitTestAggregator.
  if (!support_->SubmitCompositorFrame(local_surface_id, std::move(frame))) {
    compositor_frame_sink_binding_.CloseWithReason(
        1, "Surface invariants violation");
    OnClientConnectionLost();
  }
}

void CompositorFrameSinkImpl::DidNotProduceFrame(
    const BeginFrameAck& begin_frame_ack) {
  support_->DidNotProduceFrame(begin_frame_ack);
}

void CompositorFrameSinkImpl::DidReceiveCompositorFrameAck(
    const std::vector<ReturnedResource>& resources) {
  if (client_)
    client_->DidReceiveCompositorFrameAck(resources);
}

void CompositorFrameSinkImpl::OnBeginFrame(const BeginFrameArgs& args) {
  if (client_)
    client_->OnBeginFrame(args);
}

void CompositorFrameSinkImpl::OnBeginFramePausedChanged(bool paused) {
  if (client_)
    client_->OnBeginFramePausedChanged(paused);
}

void CompositorFrameSinkImpl::ReclaimResources(
    const std::vector<ReturnedResource>& resources) {
  if (client_)
    client_->ReclaimResources(resources);
}

void CompositorFrameSinkImpl::WillDrawSurface(
    const LocalSurfaceId& local_surface_id,
    const gfx::Rect& damage_rect) {}

void CompositorFrameSinkImpl::OnClientConnectionLost() {
  support_->frame_sink_manager()->OnClientConnectionLost(
      support_->frame_sink_id());
}

}  // namespace viz
