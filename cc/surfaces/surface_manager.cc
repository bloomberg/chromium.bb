// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/surface_manager.h"

#include "base/logging.h"

namespace cc {

SurfaceManager::SurfaceManager()
    : next_surface_id_(1) {
}

SurfaceManager::~SurfaceManager() {}

int SurfaceManager::RegisterAndAllocateIDForSurface(Surface* surface) {
  DCHECK(surface);
  int surface_id = next_surface_id_++;
  surface_map_[surface_id] = surface;
  return surface_id;
}

void SurfaceManager::DeregisterSurface(int surface_id) {
  SurfaceMap::iterator it = surface_map_.find(surface_id);
  DCHECK(it != surface_map_.end());
  surface_map_.erase(it);
}

Surface* SurfaceManager::GetSurfaceForID(int surface_id) {
  SurfaceMap::iterator it = surface_map_.find(surface_id);
  if (it == surface_map_.end())
    return NULL;
  return it->second;
}

}  // namespace cc
