// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/root_compositor_frame_sink_impl.h"

#include <utility>

#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/service/display/display.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"

namespace viz {

RootCompositorFrameSinkImpl::RootCompositorFrameSinkImpl(
    FrameSinkManagerImpl* frame_sink_manager,
    const FrameSinkId& frame_sink_id,
    std::unique_ptr<Display> display,
    std::unique_ptr<BeginFrameSource> begin_frame_source,
    mojom::CompositorFrameSinkAssociatedRequest request,
    mojom::CompositorFrameSinkClientPtr client,
    mojom::DisplayPrivateAssociatedRequest display_private_request)
    : compositor_frame_sink_client_(std::move(client)),
      compositor_frame_sink_binding_(this, std::move(request)),
      display_private_binding_(this, std::move(display_private_request)),
      support_(CompositorFrameSinkSupport::Create(
          compositor_frame_sink_client_.get(),
          frame_sink_manager,
          frame_sink_id,
          true /* is_root */,
          true /* needs_sync_points */)),
      display_begin_frame_source_(std::move(begin_frame_source)),
      display_(std::move(display)),
      hit_test_aggregator_(frame_sink_manager->hit_test_manager(), this) {
  DCHECK(display_begin_frame_source_);
  DCHECK(display_);

  compositor_frame_sink_binding_.set_connection_error_handler(
      base::Bind(&RootCompositorFrameSinkImpl::OnClientConnectionLost,
                 base::Unretained(this)));
  frame_sink_manager->RegisterBeginFrameSource(
      display_begin_frame_source_.get(), frame_sink_id);
  display_->Initialize(this, frame_sink_manager->surface_manager());
}

RootCompositorFrameSinkImpl::~RootCompositorFrameSinkImpl() {
  support_->frame_sink_manager()->UnregisterBeginFrameSource(
      display_begin_frame_source_.get());
}

void RootCompositorFrameSinkImpl::SetDisplayVisible(bool visible) {
  display_->SetVisible(visible);
}

void RootCompositorFrameSinkImpl::SetDisplayColorSpace(
    const gfx::ColorSpace& blending_color_space,
    const gfx::ColorSpace& device_color_space) {
  display_->SetColorSpace(blending_color_space, device_color_space);
}

void RootCompositorFrameSinkImpl::SetOutputIsSecure(bool secure) {
  display_->SetOutputIsSecure(secure);
}

void RootCompositorFrameSinkImpl::SetNeedsBeginFrame(bool needs_begin_frame) {
  support_->SetNeedsBeginFrame(needs_begin_frame);
}

void RootCompositorFrameSinkImpl::SubmitCompositorFrame(
    const LocalSurfaceId& local_surface_id,
    CompositorFrame frame,
    mojom::HitTestRegionListPtr hit_test_region_list,
    uint64_t submit_time) {
  // Update |display_| when size or local surface id changes.
  if (support_->local_surface_id() != local_surface_id) {
    display_->Resize(frame.size_in_pixels());
    display_->SetLocalSurfaceId(local_surface_id, frame.device_scale_factor());
  }

  if (!support_->SubmitCompositorFrame(local_surface_id, std::move(frame),
                                       std::move(hit_test_region_list))) {
    DLOG(ERROR) << "SubmitCompositorFrame failed for " << local_surface_id;
    compositor_frame_sink_binding_.Close();
    OnClientConnectionLost();
  }
}

void RootCompositorFrameSinkImpl::DidNotProduceFrame(
    const BeginFrameAck& begin_frame_ack) {
  support_->DidNotProduceFrame(begin_frame_ack);
}

void RootCompositorFrameSinkImpl::OnAggregatedHitTestRegionListUpdated(
    mojo::ScopedSharedBufferHandle active_handle,
    uint32_t active_handle_size,
    mojo::ScopedSharedBufferHandle idle_handle,
    uint32_t idle_handle_size) {
  support_->frame_sink_manager()->OnAggregatedHitTestRegionListUpdated(
      support_->frame_sink_id(), std::move(active_handle), active_handle_size,
      std::move(idle_handle), idle_handle_size);
}

void RootCompositorFrameSinkImpl::SwitchActiveAggregatedHitTestRegionList(
    uint8_t active_handle_index) {
  support_->frame_sink_manager()->SwitchActiveAggregatedHitTestRegionList(
      support_->frame_sink_id(), active_handle_index);
}

void RootCompositorFrameSinkImpl::DisplayOutputSurfaceLost() {
  // TODO(staraz): Implement this. Client should hear about context/output
  // surface lost.
}

void RootCompositorFrameSinkImpl::DisplayWillDrawAndSwap(
    bool will_draw_and_swap,
    const RenderPassList& render_pass) {
  hit_test_aggregator_.PostTaskAggregate(display_->CurrentSurfaceId());
}

void RootCompositorFrameSinkImpl::DisplayDidDrawAndSwap() {}

void RootCompositorFrameSinkImpl::OnClientConnectionLost() {
  support_->frame_sink_manager()->OnClientConnectionLost(
      support_->frame_sink_id());
}

}  // namespace viz
