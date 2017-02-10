// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/compositor_frame_sink_support.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "cc/output/compositor_frame.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/surfaces/compositor_frame_sink_support_client.h"
#include "cc/surfaces/display.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/surfaces/surface_reference.h"

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
      reference_tracker_(frame_sink_id),
      weak_factory_(this) {
  surface_manager_->RegisterFrameSinkId(frame_sink_id_);
  surface_manager_->RegisterSurfaceFactoryClient(frame_sink_id_, this);

  if (display_) {
    display_->Initialize(this, surface_manager_);
    display_->SetVisible(true);
  }
}

CompositorFrameSinkSupport::~CompositorFrameSinkSupport() {
  // For display root surfaces, the surface is no longer going to be visible
  // so make it unreachable from the top-level root.
  if (surface_manager_->using_surface_references() && display_ &&
      reference_tracker_.current_surface_id().is_valid())
    RemoveTopLevelRootReference(reference_tracker_.current_surface_id());

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
    const LocalSurfaceId& local_surface_id,
    CompositorFrame frame) {
  ++ack_pending_count_;
  float device_scale_factor = frame.metadata.device_scale_factor;

  surface_factory_.SubmitCompositorFrame(
      local_surface_id, std::move(frame),
      base::Bind(&CompositorFrameSinkSupport::DidReceiveCompositorFrameAck,
                 weak_factory_.GetWeakPtr()));

  if (surface_manager_->using_surface_references()) {
    SurfaceId last_surface_id = reference_tracker_.current_surface_id();

    // Populate list of surface references to add and remove based on reference
    // surfaces in current frame compared with the last frame. The list of
    // surface references includes references from both the pending and active
    // frame if any.
    SurfaceId current_surface_id(frame_sink_id_, local_surface_id);
    Surface* surface = surface_manager_->GetSurfaceForId(current_surface_id);
    // TODO(fsamuel): This is pretty inefficent. We copy over referenced
    // surfaces. Then we pass them into the ReferencedSurfaceTracker to copy
    // them again into a set. ReferencedSurfaceTracker should just take in two
    // vectors, one for pending referenced surfaces and one for active
    // referenced surfaces.
    std::vector<SurfaceId> referenced_surfaces =
        surface->active_referenced_surfaces();

    referenced_surfaces.insert(referenced_surfaces.end(),
                               surface->pending_referenced_surfaces().begin(),
                               surface->pending_referenced_surfaces().end());
    reference_tracker_.UpdateReferences(local_surface_id, referenced_surfaces);

    UpdateSurfaceReferences(last_surface_id, local_surface_id);
  }

  if (display_)
    display_->SetLocalSurfaceId(local_surface_id, device_scale_factor);
}

void CompositorFrameSinkSupport::Require(const LocalSurfaceId& local_surface_id,
                                         const SurfaceSequence& sequence) {
  surface_manager_->RequireSequence(SurfaceId(frame_sink_id_, local_surface_id),
                                    sequence);
}

void CompositorFrameSinkSupport::Satisfy(const SurfaceSequence& sequence) {
  surface_manager_->SatisfySequence(sequence);
}

void CompositorFrameSinkSupport::UpdateSurfaceReferences(
    const SurfaceId& last_surface_id,
    const LocalSurfaceId& local_surface_id) {
  const bool surface_id_changed =
      last_surface_id.local_surface_id() != local_surface_id;

  // If this is a display root surface and the SurfaceId is changing, make the
  // new SurfaceId reachable from the top-level root.
  if (display_ && surface_id_changed)
    AddTopLevelRootReference(reference_tracker_.current_surface_id());

  // Add references based on CompositorFrame referenced surfaces. If the
  // SurfaceId has changed all referenced surfaces will be in this list.
  if (!reference_tracker_.references_to_add().empty()) {
    surface_manager_->AddSurfaceReferences(
        reference_tracker_.references_to_add());
  }

  // If this is a display root surface and the SurfaceId is changing, make the
  // old SurfaceId unreachable from the top-level root. This needs to happen
  // after adding all references for the new SurfaceId.
  if (display_ && surface_id_changed && last_surface_id.is_valid())
    RemoveTopLevelRootReference(last_surface_id);

  // Remove references based on CompositorFrame referenced surfaces. If the
  // SurfaceId has changed this list will be empty.
  if (!reference_tracker_.references_to_remove().empty()) {
    DCHECK(!surface_id_changed);
    surface_manager_->RemoveSurfaceReferences(
        reference_tracker_.references_to_remove());
  }
}

void CompositorFrameSinkSupport::AddTopLevelRootReference(
    const SurfaceId& surface_id) {
  SurfaceReference reference(surface_manager_->GetRootSurfaceId(), surface_id);
  surface_manager_->AddSurfaceReferences({reference});
}

void CompositorFrameSinkSupport::RemoveTopLevelRootReference(
    const SurfaceId& surface_id) {
  SurfaceReference reference(surface_manager_->GetRootSurfaceId(), surface_id);
  surface_manager_->RemoveSurfaceReferences({reference});
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
    const LocalSurfaceId& local_surface_id,
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
