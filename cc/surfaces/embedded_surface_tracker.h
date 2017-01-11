// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_EMBEDDED_SURFACE_TRACKER_H_
#define CC_SURFACES_EMBEDDED_SURFACE_TRACKER_H_

#include <unordered_map>
#include <vector>

#include "base/macros.h"
#include "cc/surfaces/frame_sink_id.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_reference.h"
#include "cc/surfaces/surfaces_export.h"

namespace cc {

namespace test {
class EmbeddedSurfaceTrackerTest;
}

// Tracks surface references from a client surface to embedded surfaces. Handles
// updating the references when the client surface resizes.
class CC_SURFACES_EXPORT EmbeddedSurfaceTracker {
 public:
  EmbeddedSurfaceTracker();
  ~EmbeddedSurfaceTracker();

  const SurfaceId& current_surface_id() const { return current_surface_id_; }

  // Checks if the client has a valid SurfaceId. This will become true after
  // SetCurrentSurfaceId() is called for the first time. No references will be
  // added or removed until this is true.
  bool HasValidSurfaceId() const { return current_surface_id_.is_valid(); }

  // Sets |current_surface_id_| when the client switches surfaces. Clears all
  // references waiting to be added or removed, since no CompositorFrame will be
  // submitted for the previous surface now. Generates new references from the
  // new surface to all embedded surfaces. Does not send IPC.
  void SetCurrentSurfaceId(const SurfaceId& surface_id);

  // Adds a reference from |current_surface_id_| to |embedded_id| and records
  // |embedded_id|. If an existing surface with the same FrameSinkId is embedded
  // removes the reference to it. Does not send IPC. Returns true for the first
  // surface from a FrameSinkId and false for all subsequent surfaces.
  bool EmbedSurface(const SurfaceId& embedded_id);

  // Removes reference from |current_surface_id_| to latest embedded surface for
  // |frame_sink_id|. Does not send IPC.
  void UnembedSurface(const FrameSinkId& frame_sink_id);

  // Returns |references_to_add_| and clears contents. It's expected this will
  // be called immediately before submitting a CompositorFrame and caller will
  // add references via IPC.
  std::vector<SurfaceReference> GetReferencesToAdd();
  bool HasReferencesToAdd() const { return !references_to_add_.empty(); }

  // Returns |references_to_remove_| and clears contents. It's expected this
  // will be called immediately after submitting a CompositorFrame and caller
  // will remove references via IPC.
  std::vector<SurfaceReference> GetReferencesToRemove();
  bool HasReferencesToRemove() const { return !references_to_remove_.empty(); }

 private:
  friend class test::EmbeddedSurfaceTrackerTest;

  // Records adding a reference from |parent_id| to |child_id|. References are
  // not actually added until SendAddSurfaceReferences() is called.
  void AddReference(const SurfaceId& parent_id, const SurfaceId& child_id);

  // Records removing a reference from |parent_id| to |child_id|. References
  // are not actually removed until SendRemoveSurfaceReferences() is called.
  void RemoveReference(const SurfaceId& parent_id, const SurfaceId& child_id);

  // The id of the client surface that is embedding other surfaces.
  SurfaceId current_surface_id_;

  // Surfaces that are embedded by the client surface. A reference from
  // |current_surface_id_| will be added if it's valid.
  std::unordered_map<FrameSinkId, SurfaceId, FrameSinkIdHash>
      embedded_surfaces_;

  // References to surfaces that should be removed after the next
  // CompositorFrame has been submitted and the surfaces are no longer embedded.
  std::vector<SurfaceReference> references_to_remove_;

  // References that should be added before the next CompositorFrame is
  // submitted.
  std::vector<SurfaceReference> references_to_add_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedSurfaceTracker);
};

}  // namespace cc

#endif  // CC_SURFACES_EMBEDDED_SURFACE_TRACKER_H_
