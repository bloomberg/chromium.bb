// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/root_compositor_frame_sink_impl.h"

#include <utility>

#include "base/command_line.h"
#include "components/viz/service/display/display.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
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
    : support_(
          CompositorFrameSinkSupport::Create(this,
                                             frame_sink_manager,
                                             frame_sink_id,
                                             true /* is_root */,
                                             true /* needs_sync_points */)),
      display_begin_frame_source_(std::move(begin_frame_source)),
      display_(std::move(display)),
      client_(std::move(client)),
      compositor_frame_sink_binding_(this, std::move(request)),
      display_private_binding_(this, std::move(display_private_request)),
      hit_test_aggregator_(this) {
  DCHECK(display_begin_frame_source_);
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
  DCHECK(display_);
  display_->SetVisible(visible);
}

void RootCompositorFrameSinkImpl::ResizeDisplay(const gfx::Size& size) {
  DCHECK(display_);
  display_->Resize(size);
}

void RootCompositorFrameSinkImpl::SetDisplayColorSpace(
    const gfx::ColorSpace& color_space) {
  DCHECK(display_);
  display_->SetColorSpace(color_space, color_space);
}

void RootCompositorFrameSinkImpl::SetOutputIsSecure(bool secure) {
  DCHECK(display_);
  display_->SetOutputIsSecure(secure);
}

void RootCompositorFrameSinkImpl::SetLocalSurfaceId(
    const LocalSurfaceId& local_surface_id,
    float scale_factor) {
  display_->SetLocalSurfaceId(local_surface_id, scale_factor);
}

void RootCompositorFrameSinkImpl::SetNeedsBeginFrame(bool needs_begin_frame) {
  support_->SetNeedsBeginFrame(needs_begin_frame);
}

void RootCompositorFrameSinkImpl::SubmitCompositorFrame(
    const LocalSurfaceId& local_surface_id,
    cc::CompositorFrame frame,
    mojom::HitTestRegionListPtr hit_test_region_list,
    uint64_t submit_time) {
  // TODO(gklassen): send |hit_test_region_list| to |support_|.
  if (!support_->SubmitCompositorFrame(local_surface_id, std::move(frame))) {
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
    const cc::RenderPassList& render_pass) {
  hit_test_aggregator_.PostTaskAggregate(display_->CurrentSurfaceId());
}

void RootCompositorFrameSinkImpl::DisplayDidDrawAndSwap() {}

void RootCompositorFrameSinkImpl::DidReceiveCompositorFrameAck(
    const std::vector<ReturnedResource>& resources) {
  if (client_)
    client_->DidReceiveCompositorFrameAck(resources);
}

void RootCompositorFrameSinkImpl::OnBeginFrame(const BeginFrameArgs& args) {
  hit_test_aggregator_.Swap();
  if (client_)
    client_->OnBeginFrame(args);
}

void RootCompositorFrameSinkImpl::OnBeginFramePausedChanged(bool paused) {
  if (client_)
    client_->OnBeginFramePausedChanged(paused);
}

void RootCompositorFrameSinkImpl::ReclaimResources(
    const std::vector<ReturnedResource>& resources) {
  if (client_)
    client_->ReclaimResources(resources);
}

void RootCompositorFrameSinkImpl::WillDrawSurface(
    const LocalSurfaceId& local_surface_id,
    const gfx::Rect& damage_rect) {}

void RootCompositorFrameSinkImpl::OnClientConnectionLost() {
  support_->frame_sink_manager()->OnClientConnectionLost(
      support_->frame_sink_id());
}

}  // namespace viz
