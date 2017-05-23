// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_SURFACE_OBSERVER_H_
#define CC_SURFACES_SURFACE_OBSERVER_H_

namespace cc {

class SurfaceInfo;

class SurfaceObserver {
 public:
  // Runs when a CompositorFrame is received for the given SurfaceInfo for the
  // first time.
  virtual void OnSurfaceCreated(const SurfaceInfo& surface_info) = 0;

  // Runs when a Surface is damaged. *changed should be set to true if this
  // causes a Display to be damaged.
  virtual void OnSurfaceDamaged(const SurfaceId& surface_id, bool* changed) = 0;

  // Called when a surface is garbage-collected.
  virtual void OnSurfaceDiscarded(const SurfaceId& surface_id) = 0;
};

}  // namespace cc

#endif  // CC_SURFACES_SURFACE_OBSERVER_H_
