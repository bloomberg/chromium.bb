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
    : surface_manager_(surface_manager),
      begin_frame_source_(begin_frame_source) {
  surface_manager_->AddObserver(this);
  begin_frame_source_->AddObserver(this);
}

SurfaceDependencyTracker::~SurfaceDependencyTracker() {
  surface_manager_->RemoveObserver(this);
  begin_frame_source_->RemoveObserver(this);
  for (Surface* pending_surface : pending_surfaces_)
    pending_surface->RemoveObserver(this);
  pending_surfaces_.clear();
}

void SurfaceDependencyTracker::RequestSurfaceResolution(Surface* surface) {
  DCHECK(surface->HasPendingFrame());

  const CompositorFrame& pending_frame = surface->GetPendingFrame();
  bool needs_begin_frame =
      pending_frame.metadata.can_activate_before_dependencies;

  // Referenced surface IDs that aren't currently known to the surface manager
  // or do not have an active CompsotiorFrame block this frame.
  for (const SurfaceId& surface_id :
       pending_frame.metadata.referenced_surfaces) {
    Surface* surface_dependency = surface_manager_->GetSurfaceForId(surface_id);
    if (!surface_dependency || !surface_dependency->HasActiveFrame())
      blocked_surfaces_[surface_id].insert(surface);
  }

  if (!pending_surfaces_.count(surface)) {
    surface->AddObserver(this);
    pending_surfaces_.insert(surface);
  }

  if (needs_begin_frame && !frames_since_deadline_set_)
    frames_since_deadline_set_ = 0;
}

void SurfaceDependencyTracker::OnBeginFrame(const BeginFrameArgs& args) {
  // If no deadline is set then we have nothing to do.
  if (!frames_since_deadline_set_)
    return;

  // TODO(fsamuel, kylechar): We have a single global deadline here. We should
  // scope deadlines to surface subtrees. We cannot do that until
  // SurfaceReferences have been fully implemented
  // (see https://crbug.com/689719).
  last_begin_frame_args_ = args;
  // Nothing to do if we haven't hit a deadline yet.
  if (++(*frames_since_deadline_set_) != kMaxBeginFrameCount)
    return;

  // Activate all surfaces that respect the deadline.
  PendingSurfaceSet pending_surfaces(pending_surfaces_);
  for (Surface* pending_surface : pending_surfaces)
    pending_surface->ActivatePendingFrameForDeadline();

  frames_since_deadline_set_.reset();
}

const BeginFrameArgs& SurfaceDependencyTracker::LastUsedBeginFrameArgs() const {
  return last_begin_frame_args_;
}

void SurfaceDependencyTracker::OnBeginFrameSourcePausedChanged(bool paused) {}

void SurfaceDependencyTracker::OnReferencedSurfacesChanged(
    Surface* surface,
    const std::vector<SurfaceId>* active_referenced_surfaces,
    const std::vector<SurfaceId>* pending_referenced_surfaces) {}

void SurfaceDependencyTracker::OnSurfaceDiscarded(Surface* surface) {
  // If the surface being destroyed doesn't have a pending frame then we have
  // nothing to do here.
  if (!surface->HasPendingFrame())
    return;

  const CompositorFrame& pending_frame = surface->GetPendingFrame();

  DCHECK(!pending_frame.metadata.referenced_surfaces.empty());

  for (const SurfaceId& surface_id :
       pending_frame.metadata.referenced_surfaces) {
    auto it = blocked_surfaces_.find(surface_id);
    if (it == blocked_surfaces_.end())
      continue;

    auto& pending_surface_set = it->second;
    auto pending_surface_it = pending_surface_set.find(surface);
    if (pending_surface_it != pending_surface_set.end()) {
      pending_surface_set.erase(surface);
      if (pending_surface_set.empty())
        blocked_surfaces_.erase(surface_id);
    }
  }

  if (blocked_surfaces_.empty())
    frames_since_deadline_set_.reset();

  pending_surfaces_.erase(surface);
  surface->RemoveObserver(this);

  // Pretend that the discarded surface's SurfaceId is now available to unblock
  // dependencies because we now know the surface will never activate.
  NotifySurfaceIdAvailable(surface->surface_id());
}

void SurfaceDependencyTracker::OnSurfaceActivated(Surface* surface) {
  surface->RemoveObserver(this);
  pending_surfaces_.erase(surface);
  NotifySurfaceIdAvailable(surface->surface_id());
}

void SurfaceDependencyTracker::OnSurfaceDependenciesChanged(
    Surface* surface,
    const SurfaceDependencies& added_dependencies,
    const SurfaceDependencies& removed_dependencies) {
  // Update the |blocked_surfaces_| map with the changes in dependencies.
  for (const SurfaceId& surface_id : added_dependencies)
    blocked_surfaces_[surface_id].insert(surface);

  for (const SurfaceId& surface_id : removed_dependencies) {
    auto it = blocked_surfaces_.find(surface_id);
    it->second.erase(surface);
    if (it->second.empty())
      blocked_surfaces_.erase(it);
  }

  // If there are no more dependencies to resolve then we don't need to have a
  // deadline.
  if (blocked_surfaces_.empty())
    frames_since_deadline_set_.reset();
}

// SurfaceObserver implementation:
void SurfaceDependencyTracker::OnSurfaceCreated(
    const SurfaceInfo& surface_info) {
  // This is called when a Surface has an activated frame for the first time.
  // SurfaceDependencyTracker only observes Surfaces that contain pending
  // frames. SurfaceDependencyTracker becomes aware of CompositorFrames that
  // activate immediately go through here.
  NotifySurfaceIdAvailable(surface_info.id());
}

void SurfaceDependencyTracker::OnSurfaceDamaged(const SurfaceId& surface_id,
                                                bool* changed) {}

void SurfaceDependencyTracker::NotifySurfaceIdAvailable(
    const SurfaceId& surface_id) {
  auto it = blocked_surfaces_.find(surface_id);
  if (it == blocked_surfaces_.end())
    return;

  // Unblock surfaces that depend on this |surface_id|.
  PendingSurfaceSet blocked_pending_surface_set(it->second);
  blocked_surfaces_.erase(it);
  // If there are no more blockers in the system, then we no longer need to
  // have a deadline.
  if (blocked_surfaces_.empty())
    frames_since_deadline_set_.reset();

  // Tell each surface about the availability of its blocker.
  for (Surface* blocked_pending_surface : blocked_pending_surface_set)
    blocked_pending_surface->NotifySurfaceIdAvailable(surface_id);
}

}  // namespace cc
