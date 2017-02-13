// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_REFERENCED_SURFACE_TRACKER_H_
#define CC_SURFACES_REFERENCED_SURFACE_TRACKER_H_

#include <unordered_set>
#include <vector>

#include "base/macros.h"
#include "cc/surfaces/frame_sink_id.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_reference.h"
#include "cc/surfaces/surfaces_export.h"

namespace cc {

// Tracks surface references for a client surface. UpdateReferences() should be
// called once per CompositorFrame to populate |references_to_add_| and
// |references_to_remove_|.
class CC_SURFACES_EXPORT ReferencedSurfaceTracker {
 public:
  explicit ReferencedSurfaceTracker(const FrameSinkId& frame_sink_id);
  ~ReferencedSurfaceTracker();

  const SurfaceId& current_surface_id() const { return current_surface_id_; }

  const std::vector<SurfaceReference>& references_to_add() const {
    return references_to_add_;
  }

  const std::vector<SurfaceReference>& references_to_remove() const {
    return references_to_remove_;
  }

  // Update the references for a CompositorFrame. If |local_surface_id| has
  // changed then new references will be generated for everything in
  // |referenced_surfaces|. Otherwise a diff from the referenced surfaces in the
  // last frame will be computed. This should be called once per
  // CompositorFrame.
  void UpdateReferences(
      const LocalSurfaceId& local_surface_id,
      const std::vector<SurfaceId>* active_referenced_surfaces,
      const std::vector<SurfaceId>* pending_referenced_surfaces);

 private:
  // Updates |referenced_surfaces_| based on a |new_referenced_surfaces| from a
  // CompositorFrame. Populates |references_to_add_| and |references_to_remove_|
  // based on the difference between the sets.
  void ProcessNewReferences(const std::unordered_set<SurfaceId, SurfaceIdHash>&
                                new_referenced_surfaces);

  // Adds reference from |current_surface_id_| to |surface_id|.
  void AddSurfaceReference(const SurfaceId& surface_id);

  // Removes reference from |current_surface_id_| to |surface_id|.
  void RemoveSurfaceReference(const SurfaceId& surface_id);

  // The id of the client surface that is embedding other surfaces.
  SurfaceId current_surface_id_;

  // TODO(samans): Use the same SurfaceId set that SurfaceManager holds.
  // Set of surfaces referenced by the last submitted CompositorFrame.
  std::unordered_set<SurfaceId, SurfaceIdHash> referenced_surfaces_;

  // References to surfaces that should be added for the next CompositorFrame.
  std::vector<SurfaceReference> references_to_add_;

  // References to surfaces that should be removed after the next
  // CompositorFrame has been submitted and the surfaces are no longer needed by
  // |current_surface_id_|.
  std::vector<SurfaceReference> references_to_remove_;

  DISALLOW_COPY_AND_ASSIGN(ReferencedSurfaceTracker);
};

}  // namespace cc

#endif  // CC_SURFACES_REFERENCED_SURFACE_TRACKER_H_
