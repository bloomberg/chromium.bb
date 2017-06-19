// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_SURFACE_DEPENDENCY_TRACKER_H_
#define CC_SURFACES_SURFACE_DEPENDENCY_TRACKER_H_

#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_dependency_deadline.h"
#include "cc/surfaces/surfaces_export.h"

namespace cc {

class BeginFrameSource;
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
// TODO(fsamuel): Deadlines should not be global. They should be scoped to a
// surface subtree. However, that will not be possible until SurfaceReference
// work is complete.
class CC_SURFACES_EXPORT SurfaceDependencyTracker
    : public SurfaceDeadlineObserver {
 public:
  SurfaceDependencyTracker(SurfaceManager* surface_manager,
                           BeginFrameSource* begin_frame_source);
  ~SurfaceDependencyTracker();

  // Called when |surface| has a pending CompositorFrame and it wishes to be
  // informed when that surface's dependencies are resolved.
  void RequestSurfaceResolution(Surface* surface);

  bool has_deadline() const { return deadline_.has_deadline(); }

  // SurfaceDeadlineObserver implementation:
  void OnDeadline() override;

  void OnSurfaceActivated(Surface* surface);
  void OnSurfaceDependenciesChanged(
      Surface* surface,
      const base::flat_set<SurfaceId>& added_dependencies,
      const base::flat_set<SurfaceId>& removed_dependencies);
  void OnSurfaceDiscarded(Surface* surface);

 private:
  // Informs all Surfaces with pending frames blocked on the provided
  // |surface_id| that there is now an active frame available in Surface
  // corresponding to |surface_id|.
  void NotifySurfaceIdAvailable(const SurfaceId& surface_id);

  SurfaceManager* const surface_manager_;

  // This object tracks the deadline when all pending CompositorFrames in the
  // system will be activated.
  SurfaceDependencyDeadline deadline_;

  // A map from a SurfaceId to the set of Surfaces blocked on that SurfaceId.
  std::unordered_map<SurfaceId, base::flat_set<SurfaceId>, SurfaceIdHash>
      blocked_surfaces_from_dependency_;

  // The set of SurfaceIds corresponding that are known to have blockers.
  base::flat_set<SurfaceId> blocked_surfaces_by_id_;

  // The set of SurfaceIds to which corresponding CompositorFrames have not
  // arrived by the time their deadline fired.
  base::flat_set<SurfaceId> late_surfaces_by_id_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceDependencyTracker);
};

}  // namespace cc

#endif  // CC_SURFACES_SURFACE_DEPENDENCY_TRACKER_H_
