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
    : compositor_frame_sink_client_(std::move(client)),
      compositor_frame_sink_binding_(this, std::move(request)),
      support_(std::make_unique<CompositorFrameSinkSupport>(
          compositor_frame_sink_client_.get(),
          frame_sink_manager,
          frame_sink_id,
          false /* is_root */,
          true /* needs_sync_points */)) {
  compositor_frame_sink_binding_.set_connection_error_handler(
      base::Bind(&CompositorFrameSinkImpl::OnClientConnectionLost,
                 base::Unretained(this)));
}

CompositorFrameSinkImpl::~CompositorFrameSinkImpl() = default;

void CompositorFrameSinkImpl::SetNeedsBeginFrame(bool needs_begin_frame) {
  support_->SetNeedsBeginFrame(needs_begin_frame);
}

void CompositorFrameSinkImpl::SetWantsAnimateOnlyBeginFrames() {
  support_->SetWantsAnimateOnlyBeginFrames();
}

void CompositorFrameSinkImpl::SubmitCompositorFrame(
    const LocalSurfaceId& local_surface_id,
    CompositorFrame frame,
    mojom::HitTestRegionListPtr hit_test_region_list,
    uint64_t submit_time) {
  if (!support_->SubmitCompositorFrame(local_surface_id, std::move(frame),
                                       std::move(hit_test_region_list))) {
    DLOG(ERROR) << "SubmitCompositorFrame failed for " << local_surface_id;
    compositor_frame_sink_binding_.CloseWithReason(
        1, "Surface invariants violation");
    OnClientConnectionLost();
  }
}

void CompositorFrameSinkImpl::DidNotProduceFrame(
    const BeginFrameAck& begin_frame_ack) {
  support_->DidNotProduceFrame(begin_frame_ack);
}

void CompositorFrameSinkImpl::OnClientConnectionLost() {
  support_->frame_sink_manager()->OnClientConnectionLost(
      support_->frame_sink_id());
}

}  // namespace viz
