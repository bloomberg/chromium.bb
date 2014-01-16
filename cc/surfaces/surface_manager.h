// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_SURFACE_MANAGER_H_
#define CC_SURFACES_SURFACE_MANAGER_H_

#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "cc/surfaces/surfaces_export.h"

namespace gfx { class Size; }

namespace cc {
class CompositorFrame;
class Surface;

class CC_SURFACES_EXPORT SurfaceManager {
 public:
  SurfaceManager();
  ~SurfaceManager();

  int RegisterAndAllocateIDForSurface(Surface* surface);
  void DeregisterSurface(int surface_id);

  Surface* GetSurfaceForID(int surface_id);

 private:
  typedef base::hash_map<int, Surface*> SurfaceMap;
  SurfaceMap surface_map_;

  int next_surface_id_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceManager);
};

}  // namespace cc

#endif  // CC_SURFACES_SURFACE_MANAGER_H_
