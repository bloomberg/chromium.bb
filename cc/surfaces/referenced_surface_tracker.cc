// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/referenced_surface_tracker.h"

#include <utility>

#include "base/logging.h"

namespace cc {

ReferencedSurfaceTracker::ReferencedSurfaceTracker(
    const FrameSinkId& frame_sink_id)
    : current_surface_id_(frame_sink_id, LocalSurfaceId()) {
  DCHECK(current_surface_id_.frame_sink_id().is_valid());
}

ReferencedSurfaceTracker::~ReferencedSurfaceTracker() {}

void ReferencedSurfaceTracker::UpdateReferences(
    const LocalSurfaceId& local_surface_id,
    const std::vector<SurfaceId>* active_referenced_surfaces,
    const std::vector<SurfaceId>* pending_referenced_surfaces) {
  DCHECK(local_surface_id.is_valid());

  // Clear references to add/remove from the last frame.
  references_to_remove_.clear();
  references_to_add_.clear();

  // If |current_surface_id_| is changing then update |current_surface_id_|.
  // Also clear |referenced_surfaces_| because we haven't added any references
  // from the new SurfaceId yet.
  if (current_surface_id_.local_surface_id() != local_surface_id) {
    current_surface_id_ =
        SurfaceId(current_surface_id_.frame_sink_id(), local_surface_id);
    referenced_surfaces_.clear();
  }

  std::unordered_set<SurfaceId, SurfaceIdHash> referenced_surface_set;
  if (active_referenced_surfaces) {
    referenced_surface_set.insert(active_referenced_surfaces->begin(),
                                  active_referenced_surfaces->end());
  }

  if (pending_referenced_surfaces) {
    referenced_surface_set.insert(pending_referenced_surfaces->begin(),
                                  pending_referenced_surfaces->end());
  }

  ProcessNewReferences(referenced_surface_set);
}

void ReferencedSurfaceTracker::ProcessNewReferences(
    const std::unordered_set<SurfaceId, SurfaceIdHash>&
        new_referenced_surfaces) {
  // Removed references for each SurfaceId in |referenced_surfaces_| if they
  // aren't referenced anymore. Removing from a set invalidates iterators, so
  // create a new vector then remove everything in it.
  std::vector<SurfaceId> not_referenced;
  for (const SurfaceId& surface_id : referenced_surfaces_) {
    if (new_referenced_surfaces.count(surface_id) == 0)
      not_referenced.push_back(surface_id);
  }
  for (const SurfaceId& surface_id : not_referenced)
    RemoveSurfaceReference(surface_id);

  // Add references for each SurfaceId in |new_referenced_surfaces| if they
  // aren't already referenced.
  for (const SurfaceId& surface_id : new_referenced_surfaces) {
    if (referenced_surfaces_.count(surface_id) == 0)
      AddSurfaceReference(surface_id);
  }
}

void ReferencedSurfaceTracker::AddSurfaceReference(
    const SurfaceId& surface_id) {
  references_to_add_.push_back(
      SurfaceReference(current_surface_id_, surface_id));
  referenced_surfaces_.insert(surface_id);
}

void ReferencedSurfaceTracker::RemoveSurfaceReference(
    const SurfaceId& surface_id) {
  references_to_remove_.push_back(
      SurfaceReference(current_surface_id_, surface_id));
  referenced_surfaces_.erase(surface_id);
}

}  // namespace cc
