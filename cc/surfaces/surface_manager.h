// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_SURFACE_MANAGER_H_
#define CC_SURFACES_SURFACE_MANAGER_H_

#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "cc/surfaces/surface_damage_observer.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surfaces_export.h"

namespace cc {
class CompositorFrame;
class Surface;

class CC_SURFACES_EXPORT SurfaceManager {
 public:
  SurfaceManager();
  ~SurfaceManager();

  void RegisterSurface(Surface* surface);
  void DeregisterSurface(SurfaceId surface_id);

  Surface* GetSurfaceForId(SurfaceId surface_id);

  void AddObserver(SurfaceDamageObserver* obs) {
    observer_list_.AddObserver(obs);
  }

  void RemoveObserver(SurfaceDamageObserver* obs) {
    observer_list_.RemoveObserver(obs);
  }

  void SurfaceModified(SurfaceId surface_id);

 private:
  typedef base::hash_map<SurfaceId, Surface*> SurfaceMap;
  SurfaceMap surface_map_;
  ObserverList<SurfaceDamageObserver> observer_list_;
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceManager);
};

}  // namespace cc

#endif  // CC_SURFACES_SURFACE_MANAGER_H_
