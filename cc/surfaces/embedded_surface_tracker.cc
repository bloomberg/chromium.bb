// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/embedded_surface_tracker.h"

#include <utility>

#include "base/logging.h"

namespace cc {

EmbeddedSurfaceTracker::EmbeddedSurfaceTracker() {}

EmbeddedSurfaceTracker::~EmbeddedSurfaceTracker() {}

void EmbeddedSurfaceTracker::SetCurrentSurfaceId(const SurfaceId& surface_id) {
  DCHECK(surface_id.is_valid());
  DCHECK_NE(current_surface_id_, surface_id);

  // The last submitted CompositorFrame for |current_surface_id_| still embeds
  // the surfaces in |references_to_remove_| and hasn't embedded the surfaces in
  // |references_to_add_| yet.
  references_to_remove_.clear();
  references_to_add_.clear();

  current_surface_id_ = surface_id;

  // Add references from updated |current_surface_id_| to all embedded surfaces.
  for (auto& map_entry : embedded_surfaces_)
    AddReference(current_surface_id_, map_entry.second);
}

bool EmbeddedSurfaceTracker::EmbedSurface(const SurfaceId& embedded_id) {
  auto iter = embedded_surfaces_.find(embedded_id.frame_sink_id());

  // This is the first surface for the FrameSinkId.
  if (iter == embedded_surfaces_.end()) {
    if (HasValidSurfaceId())
      AddReference(current_surface_id_, embedded_id);

    // Add record that surface is embedded.
    embedded_surfaces_[embedded_id.frame_sink_id()] = embedded_id;
    return true;
  }

  // This is not the first surface for the FrameSinkId, we are unembedding the
  // old surface and embedding the new surface.
  SurfaceId& stored_surface_id = iter->second;
  DCHECK_NE(stored_surface_id, embedded_id);

  if (HasValidSurfaceId()) {
    RemoveReference(current_surface_id_, stored_surface_id);
    AddReference(current_surface_id_, embedded_id);
  }

  stored_surface_id = embedded_id;
  return false;
}

void EmbeddedSurfaceTracker::UnembedSurface(const FrameSinkId& frame_sink_id) {
  auto iter = embedded_surfaces_.find(frame_sink_id);
  if (iter == embedded_surfaces_.end())
    return;

  if (HasValidSurfaceId())
    RemoveReference(current_surface_id_, iter->second);

  // Remove record that surface is embedded.
  embedded_surfaces_.erase(iter);
}

std::vector<SurfaceReference> EmbeddedSurfaceTracker::GetReferencesToAdd() {
  std::vector<SurfaceReference> references;
  std::swap(references, references_to_add_);
  return references;
}

std::vector<SurfaceReference> EmbeddedSurfaceTracker::GetReferencesToRemove() {
  std::vector<SurfaceReference> references;
  std::swap(references, references_to_remove_);
  return references;
}

void EmbeddedSurfaceTracker::AddReference(const SurfaceId& parent_id,
                                          const SurfaceId& child_id) {
  references_to_add_.push_back(SurfaceReference(parent_id, child_id));
}

void EmbeddedSurfaceTracker::RemoveReference(const SurfaceId& parent_id,
                                             const SurfaceId& child_id) {
  references_to_remove_.push_back(SurfaceReference(parent_id, child_id));
}

}  // namespace cc
