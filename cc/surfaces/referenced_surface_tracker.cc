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
    const std::vector<SurfaceId>* active_referenced_surfaces) {
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

  base::flat_set<SurfaceId> new_referenced_surfaces;
  if (active_referenced_surfaces) {
    new_referenced_surfaces = base::flat_set<SurfaceId>(
        active_referenced_surfaces->begin(), active_referenced_surfaces->end(),
        base::KEEP_FIRST_OF_DUPES);
  }
  FindReferenceDiff(new_referenced_surfaces);

  if (!references_to_add_.empty() || !references_to_remove_.empty())
    swap(referenced_surfaces_, new_referenced_surfaces);
}

void ReferencedSurfaceTracker::FindReferenceDiff(
    const base::flat_set<SurfaceId>& new_referenced_surfaces) {
  // Find SurfaceIds in |referenced_surfaces_| that aren't referenced anymore.
  for (const SurfaceId& surface_id : referenced_surfaces_) {
    if (new_referenced_surfaces.count(surface_id) == 0) {
      references_to_remove_.push_back(
          SurfaceReference(current_surface_id_, surface_id));
    }
  }

  // Find SurfaceIds in |new_referenced_surfaces| that aren't already
  // referenced.
  for (const SurfaceId& surface_id : new_referenced_surfaces) {
    if (referenced_surfaces_.count(surface_id) == 0) {
      references_to_add_.push_back(
          SurfaceReference(current_surface_id_, surface_id));
    }
  }
}

}  // namespace cc
