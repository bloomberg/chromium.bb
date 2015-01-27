// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/surface_manager.h"

#include "base/logging.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_id_allocator.h"

namespace cc {

SurfaceManager::SurfaceManager() {
  thread_checker_.DetachFromThread();
}

SurfaceManager::~SurfaceManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (SurfaceDestroyList::iterator it = surfaces_to_destroy_.begin();
       it != surfaces_to_destroy_.end();
       ++it) {
    DeregisterSurface((*it)->surface_id());
    delete *it;
  }
}

void SurfaceManager::RegisterSurface(Surface* surface) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(surface);
  DCHECK(!surface_map_.count(surface->surface_id()));
  surface_map_[surface->surface_id()] = surface;
}

void SurfaceManager::DeregisterSurface(SurfaceId surface_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  SurfaceMap::iterator it = surface_map_.find(surface_id);
  DCHECK(it != surface_map_.end());
  surface_map_.erase(it);
}

void SurfaceManager::Destroy(scoped_ptr<Surface> surface) {
  DCHECK(thread_checker_.CalledOnValidThread());
  surfaces_to_destroy_.push_back(surface.release());
  SearchForSatisfaction();
}

void SurfaceManager::DidSatisfySequences(uint32_t id_namespace,
                                         std::vector<uint32_t>* sequence) {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (std::vector<uint32_t>::iterator it = sequence->begin();
       it != sequence->end();
       ++it) {
    satisfied_sequences_.insert(SurfaceSequence(id_namespace, *it));
  }
  sequence->clear();
  SearchForSatisfaction();
}

void SurfaceManager::SearchForSatisfaction() {
  for (SurfaceDestroyList::iterator dest_it = surfaces_to_destroy_.begin();
       dest_it != surfaces_to_destroy_.end();) {
    (*dest_it)->SatisfyDestructionDependencies(&satisfied_sequences_);
    if (!(*dest_it)->GetDestructionDependencyCount()) {
      scoped_ptr<Surface> surf(*dest_it);
      DeregisterSurface(surf->surface_id());
      dest_it = surfaces_to_destroy_.erase(dest_it);
    } else {
      ++dest_it;
    }
  }
}

Surface* SurfaceManager::GetSurfaceForId(SurfaceId surface_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  SurfaceMap::iterator it = surface_map_.find(surface_id);
  if (it == surface_map_.end())
    return NULL;
  return it->second;
}

bool SurfaceManager::SurfaceModified(SurfaceId surface_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  bool changed = false;
  FOR_EACH_OBSERVER(SurfaceDamageObserver, observer_list_,
                    OnSurfaceDamaged(surface_id, &changed));
  return changed;
}

}  // namespace cc
