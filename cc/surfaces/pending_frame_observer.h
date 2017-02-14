// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_PENDING_FRAME_OBSERVER_H_
#define CC_SURFACES_PENDING_FRAME_OBSERVER_H_

#include "base/containers/flat_set.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surfaces_export.h"

namespace cc {

using SurfaceDependencies = base::flat_set<SurfaceId>;

class Surface;

// Clients that wish to observe when the state of a pending CompostiorFrame
// changes should implement this class.
class CC_SURFACES_EXPORT PendingFrameObserver {
 public:
  // Called when one or both of the sets of referenced surfaces have changed.
  virtual void OnReferencedSurfacesChanged(
      Surface* surface,
      const std::vector<SurfaceId>* active_referenced_surfaces,
      const std::vector<SurfaceId>* pending_referenced_surfaces) = 0;

  // Called when a CompositorFrame within |surface| has activated.
  virtual void OnSurfaceActivated(Surface* surface) = 0;

  // Called when the dependencies of a pending CompositorFrame within |surface|
  // have changed.
  virtual void OnSurfaceDependenciesChanged(
      Surface* surface,
      const SurfaceDependencies& added_dependencies,
      const SurfaceDependencies& removed_dependencies) = 0;

  // Called when |surface| is being destroyed.
  virtual void OnSurfaceDiscarded(Surface* surface) = 0;

 protected:
  virtual ~PendingFrameObserver() = default;
};

}  // namespace cc

#endif  // CC_SURFACES_PENDING_FRAME_OBSERVER_H_
