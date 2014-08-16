// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/surface_manager.h"

#include "base/logging.h"
#include "cc/surfaces/surface.h"

namespace cc {

SurfaceManager::SurfaceManager() {
}

SurfaceManager::~SurfaceManager() {
  DCHECK_EQ(0u, surface_map_.size());
}

void SurfaceManager::RegisterSurface(Surface* surface) {
  DCHECK(surface);
  DCHECK(!surface_map_.count(surface->surface_id()));
  surface_map_[surface->surface_id()] = surface;
}

void SurfaceManager::DeregisterSurface(SurfaceId surface_id) {
  SurfaceMap::iterator it = surface_map_.find(surface_id);
  DCHECK(it != surface_map_.end());
  surface_map_.erase(it);
}

Surface* SurfaceManager::GetSurfaceForId(SurfaceId surface_id) {
  SurfaceMap::iterator it = surface_map_.find(surface_id);
  if (it == surface_map_.end())
    return NULL;
  return it->second;
}

void SurfaceManager::SurfaceModified(SurfaceId surface_id) {
  FOR_EACH_OBSERVER(
      SurfaceDamageObserver, observer_list_, OnSurfaceDamaged(surface_id));
}

}  // namespace cc
