// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_PENDING_FRAME_OBSERVER_H_
#define CC_SURFACES_PENDING_FRAME_OBSERVER_H_

#include "base/containers/flat_set.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surfaces_export.h"

namespace cc {

class Surface;

// Clients that wish to observe when the state of a pending CompostiorFrame
// changes should implement this class.
class CC_SURFACES_EXPORT PendingFrameObserver {
 public:
  // Called when a CompositorFrame within |surface| has activated.
  virtual void OnSurfaceActivated(Surface* surface) = 0;

  // Called when the dependencies of a pending CompositorFrame within |surface|
  // have changed.
  virtual void OnSurfaceDependenciesChanged(
      Surface* surface,
      const base::flat_set<SurfaceId>& added_dependencies,
      const base::flat_set<SurfaceId>& removed_dependencies) = 0;

  // Called when |surface| is being destroyed.
  virtual void OnSurfaceDiscarded(Surface* surface) = 0;

 protected:
  virtual ~PendingFrameObserver() = default;
};

}  // namespace cc

#endif  // CC_SURFACES_PENDING_FRAME_OBSERVER_H_
