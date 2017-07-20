// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_SURFACE_DEPENDENCY_TRACKER_H_
#define CC_SURFACES_SURFACE_DEPENDENCY_TRACKER_H_

#include "cc/surfaces/surface.h"
#include "cc/surfaces/surfaces_export.h"

namespace cc {

class SurfaceManager;

// SurfaceDependencyTracker tracks unresolved dependencies blocking
// CompositorFrames from activating. This class maintains a map from
// a dependent surface ID to a set of Surfaces that have CompositorFrames
// blocked on that surface ID. SurfaceDependencyTracker observes when
// dependent frames activate, and informs blocked surfaces.
//
// When a blocking CompositorFrame is first submitted, SurfaceDependencyTracker
// will begin listening for BeginFrames, setting a deadline some number of
// BeginFrames in the future. If there are unresolved dependencies when the
// deadline hits, then SurfaceDependencyTracker will clear then and activate
// all pending CompositorFrames. Once there are no more remaining pending
// frames, then SurfaceDependencyTracker will stop observing BeginFrames.
class CC_SURFACES_EXPORT SurfaceDependencyTracker {
 public:
  explicit SurfaceDependencyTracker(SurfaceManager* surface_manager);
  ~SurfaceDependencyTracker();

  // Called when |surface| has a pending CompositorFrame and it wishes to be
  // informed when that surface's dependencies are resolved.
  void RequestSurfaceResolution(Surface* surface);

  void OnSurfaceActivated(Surface* surface);
  void OnSurfaceDependenciesChanged(
      Surface* surface,
      const base::flat_set<viz::SurfaceId>& added_dependencies,
      const base::flat_set<viz::SurfaceId>& removed_dependencies);
  void OnSurfaceDiscarded(Surface* surface);

 private:
  // If |surface| has a dependent embedder frame, then it inherits the parent's
  // deadline and propagates that deadline to children.
  void UpdateSurfaceDeadline(Surface* surface);

  // Activates this |surface| and its entire dependency tree.
  void ActivateLateSurfaceSubtree(Surface* surface);

  // Indicates whether |surface| is late. A surface is late if it hasn't had its
  // first activation before a embedder is forced to activate its own
  // CompositorFrame. A surface may no longer be considered late if the set of
  // activation dependencies for dependent surfaces change.
  bool IsSurfaceLate(Surface* surface);

  // Informs all Surfaces with pending frames blocked on the provided
  // |surface_id| that there is now an active frame available in Surface
  // corresponding to |surface_id|.
  void NotifySurfaceIdAvailable(const viz::SurfaceId& surface_id);

  SurfaceManager* const surface_manager_;

  // A map from a SurfaceId to the set of Surfaces blocked on that SurfaceId.
  std::unordered_map<viz::SurfaceId,
                     base::flat_set<viz::SurfaceId>,
                     viz::SurfaceIdHash>
      blocked_surfaces_from_dependency_;

  // The set of SurfaceIds corresponding that are known to have blockers.
  base::flat_set<viz::SurfaceId> blocked_surfaces_by_id_;

  // The set of SurfaceIds corresponding to Surfaces that have active
  // CompositorFrames with missing dependencies.
  base::flat_set<viz::SurfaceId> surfaces_with_missing_dependencies_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceDependencyTracker);
};

}  // namespace cc

#endif  // CC_SURFACES_SURFACE_DEPENDENCY_TRACKER_H_
