// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/compositor_frame_sink_support.h"

#include "cc/output/compositor_frame.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/surfaces/compositor_frame_sink_support_client.h"
#include "cc/surfaces/display.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_manager.h"

namespace cc {

CompositorFrameSinkSupport::CompositorFrameSinkSupport(
    CompositorFrameSinkSupportClient* client,
    SurfaceManager* surface_manager,
    const FrameSinkId& frame_sink_id,
    std::unique_ptr<Display> display,
    std::unique_ptr<BeginFrameSource> display_begin_frame_source)
    : client_(client),
      surface_manager_(surface_manager),
      frame_sink_id_(frame_sink_id),
      display_begin_frame_source_(std::move(display_begin_frame_source)),
      display_(std::move(display)),
      surface_factory_(frame_sink_id_, surface_manager_, this),
      weak_factory_(this) {
  surface_manager_->RegisterFrameSinkId(frame_sink_id_);
  surface_manager_->RegisterSurfaceFactoryClient(frame_sink_id_, this);

  if (display_) {
    display_->Initialize(this, surface_manager_);
    display_->SetVisible(true);
  }
}

CompositorFrameSinkSupport::~CompositorFrameSinkSupport() {
  for (auto& child_frame_sink_id : child_frame_sinks_) {
    DCHECK(child_frame_sink_id.is_valid());
    surface_manager_->UnregisterFrameSinkHierarchy(frame_sink_id_,
                                                   child_frame_sink_id);
  }
  // SurfaceFactory's destructor will attempt to return resources which will
  // call back into here and access |client_| so we should destroy
  // |surface_factory_|'s resources early on.
  surface_factory_.EvictSurface();
  surface_manager_->UnregisterSurfaceFactoryClient(frame_sink_id_);
  surface_manager_->InvalidateFrameSinkId(frame_sink_id_);
}

void CompositorFrameSinkSupport::EvictFrame() {
  surface_factory_.EvictSurface();
}

void CompositorFrameSinkSupport::SetNeedsBeginFrame(bool needs_begin_frame) {
  needs_begin_frame_ = needs_begin_frame;
  UpdateNeedsBeginFramesInternal();
}

void CompositorFrameSinkSupport::SubmitCompositorFrame(
    const LocalFrameId& local_frame_id,
    CompositorFrame frame) {
  ++ack_pending_count_;
  surface_factory_.SubmitCompositorFrame(
      local_frame_id, std::move(frame),
      base::Bind(&CompositorFrameSinkSupport::DidReceiveCompositorFrameAck,
                 weak_factory_.GetWeakPtr()));
  if (display_) {
    display_->SetLocalFrameId(local_frame_id,
                              frame.metadata.device_scale_factor);
  }
}

void CompositorFrameSinkSupport::Require(const LocalFrameId& local_frame_id,
                                         const SurfaceSequence& sequence) {
  surface_manager_->RequireSequence(SurfaceId(frame_sink_id_, local_frame_id),
                                    sequence);
}

void CompositorFrameSinkSupport::Satisfy(const SurfaceSequence& sequence) {
  surface_manager_->SatisfySequence(sequence);
}

void CompositorFrameSinkSupport::DidReceiveCompositorFrameAck() {
  DCHECK_GT(ack_pending_count_, 0);
  ack_pending_count_--;

  if (!client_)
    return;
  client_->DidReceiveCompositorFrameAck();
  if (!surface_returned_resources_.empty()) {
    client_->ReclaimResources(surface_returned_resources_);
    surface_returned_resources_.clear();
  }
}

void CompositorFrameSinkSupport::AddChildFrameSink(
    const FrameSinkId& child_frame_sink_id) {
  child_frame_sinks_.insert(child_frame_sink_id);
  surface_manager_->RegisterFrameSinkHierarchy(frame_sink_id_,
                                               child_frame_sink_id);
}

void CompositorFrameSinkSupport::RemoveChildFrameSink(
    const FrameSinkId& child_frame_sink_id) {
  auto it = child_frame_sinks_.find(child_frame_sink_id);
  DCHECK(it != child_frame_sinks_.end());
  DCHECK(it->is_valid());
  surface_manager_->UnregisterFrameSinkHierarchy(frame_sink_id_,
                                                 child_frame_sink_id);
  child_frame_sinks_.erase(it);
}

void CompositorFrameSinkSupport::DisplayOutputSurfaceLost() {}

void CompositorFrameSinkSupport::DisplayWillDrawAndSwap(
    bool will_draw_and_swap,
    const RenderPassList& render_passes) {}

void CompositorFrameSinkSupport::DisplayDidDrawAndSwap() {}

void CompositorFrameSinkSupport::ReturnResources(
    const ReturnedResourceArray& resources) {
  if (resources.empty())
    return;

  if (!ack_pending_count_ && client_) {
    client_->ReclaimResources(resources);
    return;
  }

  std::copy(resources.begin(), resources.end(),
            std::back_inserter(surface_returned_resources_));
}

void CompositorFrameSinkSupport::SetBeginFrameSource(
    BeginFrameSource* begin_frame_source) {
  if (begin_frame_source_ && added_frame_observer_) {
    begin_frame_source_->RemoveObserver(this);
    added_frame_observer_ = false;
  }
  begin_frame_source_ = begin_frame_source;
  UpdateNeedsBeginFramesInternal();
}

void CompositorFrameSinkSupport::WillDrawSurface(
    const LocalFrameId& local_frame_id,
    const gfx::Rect& damage_rect) {
  if (client_)
    client_->WillDrawSurface();
}

void CompositorFrameSinkSupport::OnBeginFrame(const BeginFrameArgs& args) {
  UpdateNeedsBeginFramesInternal();
  last_begin_frame_args_ = args;
  if (client_)
    client_->OnBeginFrame(args);
}

const BeginFrameArgs& CompositorFrameSinkSupport::LastUsedBeginFrameArgs()
    const {
  return last_begin_frame_args_;
}

void CompositorFrameSinkSupport::OnBeginFrameSourcePausedChanged(bool paused) {}

void CompositorFrameSinkSupport::UpdateNeedsBeginFramesInternal() {
  if (!begin_frame_source_)
    return;

  if (needs_begin_frame_ == added_frame_observer_)
    return;

  added_frame_observer_ = needs_begin_frame_;
  if (needs_begin_frame_)
    begin_frame_source_->AddObserver(this);
  else
    begin_frame_source_->RemoveObserver(this);
}

}  // namespace cc
