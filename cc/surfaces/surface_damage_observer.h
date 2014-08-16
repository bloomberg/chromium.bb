// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_SURFACE_DAMAGE_OBSERVER_H_
#define CC_SURFACES_SURFACE_DAMAGE_OBSERVER_H_

#include "cc/surfaces/surface_id.h"

namespace cc {

class SurfaceDamageObserver {
 public:
  virtual void OnSurfaceDamaged(SurfaceId surface_id) = 0;
};

}  // namespace cc

#endif  // CC_SURFACES_SURFACE_DAMAGE_OBSERVER_H_
