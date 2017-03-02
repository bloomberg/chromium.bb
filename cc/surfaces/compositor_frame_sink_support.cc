// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/compositor_frame_sink_support.h"

#include <algorithm>
#include <utility>

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
    bool is_root,
    bool handles_frame_sink_id_invalidation,
    bool needs_sync_points)
    : client_(client),
      surface_manager_(surface_manager),
      frame_sink_id_(frame_sink_id),
      surface_factory_(frame_sink_id_, surface_manager_, this),
      reference_tracker_(frame_sink_id),
      is_root_(is_root),
      handles_frame_sink_id_invalidation_(handles_frame_sink_id_invalidation),
      weak_factory_(this) {
  surface_factory_.set_needs_sync_points(needs_sync_points);
  if (handles_frame_sink_id_invalidation_)
    surface_manager_->RegisterFrameSinkId(frame_sink_id_);
  surface_manager_->RegisterSurfaceFactoryClient(frame_sink_id_, this);
}

CompositorFrameSinkSupport::~CompositorFrameSinkSupport() {
  // Unregister |this| as a BeginFrameObserver so that the BeginFrameSource does
  // not call into |this| after it's deleted.
  SetNeedsBeginFrame(false);

  // For display root surfaces, the surface is no longer going to be visible
  // so make it unreachable from the top-level root.
  if (surface_manager_->using_surface_references() && is_root_ &&
      reference_tracker_.current_surface_id().is_valid())
    RemoveTopLevelRootReference(reference_tracker_.current_surface_id());

  // SurfaceFactory's destructor will attempt to return resources which will
  // call back into here and access |client_| so we should destroy
  // |surface_factory_|'s resources early on.
  surface_factory_.EvictSurface();
  surface_manager_->UnregisterSurfaceFactoryClient(frame_sink_id_);
  if (handles_frame_sink_id_invalidation_)
    surface_manager_->InvalidateFrameSinkId(frame_sink_id_);
}

void CompositorFrameSinkSupport::EvictFrame() {
  surface_factory_.EvictSurface();
}

void CompositorFrameSinkSupport::SetNeedsBeginFrame(bool needs_begin_frame) {
  needs_begin_frame_ = needs_begin_frame;
  UpdateNeedsBeginFramesInternal();
}

void CompositorFrameSinkSupport::DidFinishFrame(const BeginFrameAck& ack) {
  if (begin_frame_source_)
    begin_frame_source_->DidFinishFrame(this, ack);
}

void CompositorFrameSinkSupport::SubmitCompositorFrame(
    const LocalSurfaceId& local_surface_id,
    CompositorFrame frame) {
  ++ack_pending_count_;

  surface_factory_.SubmitCompositorFrame(
      local_surface_id, std::move(frame),
      base::Bind(&CompositorFrameSinkSupport::DidReceiveCompositorFrameAck,
                 weak_factory_.GetWeakPtr()));
}

void CompositorFrameSinkSupport::UpdateSurfaceReferences(
    const SurfaceId& last_surface_id,
    const LocalSurfaceId& local_surface_id) {
  const bool surface_id_changed =
      last_surface_id.local_surface_id() != local_surface_id;

  // If this is a display root surface and the SurfaceId is changing, make the
  // new SurfaceId reachable from the top-level root.
  if (is_root_ && surface_id_changed)
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
  if (is_root_ && surface_id_changed && last_surface_id.is_valid())
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

void CompositorFrameSinkSupport::ForceReclaimResources() {
  surface_factory_.ClearSurface();
}

void CompositorFrameSinkSupport::ClaimTemporaryReference(
    const SurfaceId& surface_id) {
  surface_manager_->AssignTemporaryReference(surface_id, frame_sink_id_);
}

void CompositorFrameSinkSupport::ReferencedSurfacesChanged(
    const LocalSurfaceId& local_surface_id,
    const std::vector<SurfaceId>* active_referenced_surfaces,
    const std::vector<SurfaceId>* pending_referenced_surfaces) {
  if (!surface_manager_->using_surface_references())
    return;

  SurfaceId last_surface_id = reference_tracker_.current_surface_id();

  // Populate list of surface references to add and remove based on reference
  // surfaces in current frame compared with the last frame. The list of
  // surface references includes references from both the pending and active
  // frame if any.
  reference_tracker_.UpdateReferences(local_surface_id,
                                      active_referenced_surfaces,
                                      pending_referenced_surfaces);

  UpdateSurfaceReferences(last_surface_id, local_surface_id);
}

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
    client_->WillDrawSurface(local_surface_id, damage_rect);
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

void CompositorFrameSinkSupport::RequestCopyOfSurface(
    std::unique_ptr<CopyOutputRequest> request) {
  surface_factory_.RequestCopyOfSurface(std::move(request));
}

}  // namespace cc
