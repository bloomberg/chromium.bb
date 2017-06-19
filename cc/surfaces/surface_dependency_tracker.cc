// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/surface_dependency_tracker.h"

#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_info.h"
#include "cc/surfaces/surface_manager.h"

namespace cc {

namespace {
constexpr uint32_t kMaxBeginFrameCount = 4;
}

SurfaceDependencyTracker::SurfaceDependencyTracker(
    SurfaceManager* surface_manager,
    BeginFrameSource* begin_frame_source)
    : surface_manager_(surface_manager), deadline_(begin_frame_source) {
  deadline_.AddObserver(this);
}

SurfaceDependencyTracker::~SurfaceDependencyTracker() {
  deadline_.Cancel();
  deadline_.RemoveObserver(this);
}

void SurfaceDependencyTracker::RequestSurfaceResolution(Surface* surface) {
  DCHECK(surface->HasPendingFrame());

  const CompositorFrame& pending_frame = surface->GetPendingFrame();
  bool needs_deadline = pending_frame.metadata.can_activate_before_dependencies;

  auto late_it = late_surfaces_by_id_.find(surface->surface_id());
  if (needs_deadline && late_it != late_surfaces_by_id_.end()) {
    late_surfaces_by_id_.erase(late_it);
    surface->ActivatePendingFrameForDeadline();
    return;
  }

  // Activation dependencies that aren't currently known to the surface manager
  // or do not have an active CompsotiorFrame block this frame.
  for (const SurfaceId& surface_id :
       pending_frame.metadata.activation_dependencies) {
    Surface* dependency = surface_manager_->GetSurfaceForId(surface_id);
    if (!dependency || !dependency->HasActiveFrame())
      blocked_surfaces_from_dependency_[surface_id].insert(
          surface->surface_id());
  }

  blocked_surfaces_by_id_.insert(surface->surface_id());

  if (needs_deadline && !deadline_.has_deadline())
    deadline_.Set(kMaxBeginFrameCount);
}

void SurfaceDependencyTracker::OnSurfaceActivated(Surface* surface) {
  blocked_surfaces_by_id_.erase(surface->surface_id());
  NotifySurfaceIdAvailable(surface->surface_id());
}

void SurfaceDependencyTracker::OnSurfaceDependenciesChanged(
    Surface* surface,
    const base::flat_set<SurfaceId>& added_dependencies,
    const base::flat_set<SurfaceId>& removed_dependencies) {
  // Update the |blocked_surfaces_from_dependency_| map with the changes in
  // dependencies.
  for (const SurfaceId& surface_id : added_dependencies)
    blocked_surfaces_from_dependency_[surface_id].insert(surface->surface_id());

  for (const SurfaceId& surface_id : removed_dependencies) {
    auto it = blocked_surfaces_from_dependency_.find(surface_id);
    it->second.erase(surface->surface_id());
    if (it->second.empty())
      blocked_surfaces_from_dependency_.erase(it);
  }

  // If there are no more dependencies to resolve then we don't need to have a
  // deadline.
  if (blocked_surfaces_from_dependency_.empty())
    deadline_.Cancel();
}

void SurfaceDependencyTracker::OnSurfaceDiscarded(Surface* surface) {
  // If the surface being destroyed doesn't have a pending frame then we have
  // nothing to do here.
  if (!surface->HasPendingFrame())
    return;

  const CompositorFrame& pending_frame = surface->GetPendingFrame();

  DCHECK(!pending_frame.metadata.activation_dependencies.empty());

  for (const SurfaceId& surface_id :
       pending_frame.metadata.activation_dependencies) {
    auto it = blocked_surfaces_from_dependency_.find(surface_id);
    if (it == blocked_surfaces_from_dependency_.end())
      continue;

    auto& blocked_surface_ids = it->second;
    auto blocked_surface_ids_it =
        blocked_surface_ids.find(surface->surface_id());
    if (blocked_surface_ids_it != blocked_surface_ids.end()) {
      blocked_surface_ids.erase(surface->surface_id());
      if (blocked_surface_ids.empty())
        blocked_surfaces_from_dependency_.erase(surface_id);
    }
  }

  if (blocked_surfaces_from_dependency_.empty())
    deadline_.Cancel();

  blocked_surfaces_by_id_.erase(surface->surface_id());

  // Pretend that the discarded surface's SurfaceId is now available to unblock
  // dependencies because we now know the surface will never activate.
  NotifySurfaceIdAvailable(surface->surface_id());
}

void SurfaceDependencyTracker::OnDeadline() {
  late_surfaces_by_id_.clear();

  // Activate all surfaces that respect the deadline.
  // Copy the set of blocked surfaces here because that set can mutate as we
  // activate CompositorFrames: an activation can trigger further activations
  // which will remove elements from |blocked_surfaces_by_id_|. This
  // invalidates the iterator.
  base::flat_set<SurfaceId> blocked_surfaces_by_id(blocked_surfaces_by_id_);
  for (const SurfaceId& surface_id : blocked_surfaces_by_id) {
    Surface* blocked_surface = surface_manager_->GetSurfaceForId(surface_id);
    if (!blocked_surface) {
      // A blocked surface may have been garbage collected during dependency
      // resolution.
      DCHECK(!blocked_surfaces_by_id_.count(surface_id));
      continue;
    }
    // Clear all tracked blockers for |blocked_surface|.
    for (const SurfaceId& blocking_surface_id :
         blocked_surface->blocking_surfaces()) {
      // If we are not activating this blocker now, then it's late.
      if (!blocked_surfaces_by_id.count(blocking_surface_id))
        late_surfaces_by_id_.insert(blocking_surface_id);
      blocked_surfaces_from_dependency_[blocking_surface_id].erase(surface_id);
    }
    blocked_surface->ActivatePendingFrameForDeadline();
  }
}

void SurfaceDependencyTracker::NotifySurfaceIdAvailable(
    const SurfaceId& surface_id) {
  auto it = blocked_surfaces_from_dependency_.find(surface_id);
  if (it == blocked_surfaces_from_dependency_.end())
    return;

  // Unblock surfaces that depend on this |surface_id|.
  base::flat_set<SurfaceId> blocked_surfaces_by_id(it->second);
  blocked_surfaces_from_dependency_.erase(it);
  // If there are no more blockers in the system, then we no longer need to
  // have a deadline.
  if (blocked_surfaces_from_dependency_.empty())
    deadline_.Cancel();

  // Tell each surface about the availability of its blocker.
  for (const SurfaceId& blocked_surface_by_id : blocked_surfaces_by_id) {
    Surface* blocked_surface =
        surface_manager_->GetSurfaceForId(blocked_surface_by_id);
    if (!blocked_surface) {
      // A blocked surface may have been garbage collected during dependency
      // resolution.
      DCHECK(!blocked_surfaces_by_id_.count(blocked_surface_by_id));
      continue;
    }
    blocked_surface->NotifySurfaceIdAvailable(surface_id);
  }
}

}  // namespace cc
