// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/surface_manager.h"

#include <stddef.h>
#include <stdint.h>

#include "base/logging.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_factory_client.h"
#include "cc/surfaces/surface_id_allocator.h"

namespace cc {

SurfaceManager::FrameSinkSourceMapping::FrameSinkSourceMapping()
    : client(nullptr), source(nullptr) {}

SurfaceManager::FrameSinkSourceMapping::FrameSinkSourceMapping(
    const FrameSinkSourceMapping& other) = default;

SurfaceManager::FrameSinkSourceMapping::~FrameSinkSourceMapping() {
  DCHECK(is_empty()) << "client: " << client
                     << ", children: " << children.size();
}

SurfaceManager::SurfaceManager() {
  thread_checker_.DetachFromThread();
}

SurfaceManager::~SurfaceManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (SurfaceDestroyList::iterator it = surfaces_to_destroy_.begin();
       it != surfaces_to_destroy_.end();
       ++it) {
    DeregisterSurface((*it)->surface_id());
  }
  surfaces_to_destroy_.clear();

  // All hierarchies, sources, and surface factory clients should be
  // unregistered prior to SurfaceManager destruction.
  DCHECK_EQ(frame_sink_source_map_.size(), 0u);
  DCHECK_EQ(registered_sources_.size(), 0u);
}

void SurfaceManager::RegisterSurface(Surface* surface) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(surface);
  DCHECK(!surface_map_.count(surface->surface_id()));
  surface_map_[surface->surface_id()] = surface;
}

void SurfaceManager::DeregisterSurface(const SurfaceId& surface_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  SurfaceMap::iterator it = surface_map_.find(surface_id);
  DCHECK(it != surface_map_.end());
  surface_map_.erase(it);
}

void SurfaceManager::Destroy(std::unique_ptr<Surface> surface) {
  DCHECK(thread_checker_.CalledOnValidThread());
  surface->set_destroyed(true);
  surfaces_to_destroy_.push_back(std::move(surface));
  GarbageCollectSurfaces();
}

void SurfaceManager::DidSatisfySequences(const FrameSinkId& frame_sink_id,
                                         std::vector<uint32_t>* sequence) {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (std::vector<uint32_t>::iterator it = sequence->begin();
       it != sequence->end();
       ++it) {
    satisfied_sequences_.insert(SurfaceSequence(frame_sink_id, *it));
  }
  sequence->clear();
  GarbageCollectSurfaces();
}

void SurfaceManager::RegisterFrameSinkId(const FrameSinkId& frame_sink_id) {
  bool inserted = valid_frame_sink_ids_.insert(frame_sink_id).second;
  DCHECK(inserted);
}

void SurfaceManager::InvalidateFrameSinkId(const FrameSinkId& frame_sink_id) {
  valid_frame_sink_ids_.erase(frame_sink_id);
  GarbageCollectSurfaces();
}

void SurfaceManager::GarbageCollectSurfaces() {
  // Simple mark and sweep GC.
  // TODO(jbauman): Reduce the amount of work when nothing needs to be
  // destroyed.
  std::vector<SurfaceId> live_surfaces;
  std::set<SurfaceId> live_surfaces_set;

  // GC roots are surfaces that have not been destroyed, or have not had all
  // their destruction dependencies satisfied.
  for (auto& map_entry : surface_map_) {
    map_entry.second->SatisfyDestructionDependencies(&satisfied_sequences_,
                                                     &valid_frame_sink_ids_);
    if (!map_entry.second->destroyed() ||
        map_entry.second->GetDestructionDependencyCount()) {
      live_surfaces_set.insert(map_entry.first);
      live_surfaces.push_back(map_entry.first);
    }
  }

  // Mark all surfaces reachable from live surfaces by adding them to
  // live_surfaces and live_surfaces_set.
  for (size_t i = 0; i < live_surfaces.size(); i++) {
    Surface* surf = surface_map_[live_surfaces[i]];
    DCHECK(surf);

    for (const SurfaceId& id : surf->referenced_surfaces()) {
      if (live_surfaces_set.count(id))
        continue;

      Surface* surf2 = GetSurfaceForId(id);
      if (surf2) {
        live_surfaces.push_back(id);
        live_surfaces_set.insert(id);
      }
    }
  }

  std::vector<std::unique_ptr<Surface>> to_destroy;

  // Destroy all remaining unreachable surfaces.
  for (SurfaceDestroyList::iterator dest_it = surfaces_to_destroy_.begin();
       dest_it != surfaces_to_destroy_.end();) {
    if (!live_surfaces_set.count((*dest_it)->surface_id())) {
      std::unique_ptr<Surface> surf(std::move(*dest_it));
      DeregisterSurface(surf->surface_id());
      dest_it = surfaces_to_destroy_.erase(dest_it);
      to_destroy.push_back(std::move(surf));
    } else {
      ++dest_it;
    }
  }

  to_destroy.clear();
}

void SurfaceManager::RegisterSurfaceFactoryClient(
    const FrameSinkId& frame_sink_id,
    SurfaceFactoryClient* client) {
  DCHECK(client);
  DCHECK(!frame_sink_source_map_[frame_sink_id].client);
  DCHECK_EQ(valid_frame_sink_ids_.count(frame_sink_id), 1u);

  auto iter = frame_sink_source_map_.find(frame_sink_id);
  if (iter == frame_sink_source_map_.end()) {
    auto insert_result = frame_sink_source_map_.insert(
        std::make_pair(frame_sink_id, FrameSinkSourceMapping()));
    DCHECK(insert_result.second);
    iter = insert_result.first;
  }
  iter->second.client = client;

  // Propagate any previously set sources to the new client.
  if (iter->second.source)
    client->SetBeginFrameSource(iter->second.source);
}

void SurfaceManager::UnregisterSurfaceFactoryClient(
    const FrameSinkId& frame_sink_id) {
  DCHECK_EQ(valid_frame_sink_ids_.count(frame_sink_id), 1u);
  DCHECK_EQ(frame_sink_source_map_.count(frame_sink_id), 1u);

  auto iter = frame_sink_source_map_.find(frame_sink_id);
  if (iter->second.source)
    iter->second.client->SetBeginFrameSource(nullptr);
  iter->second.client = nullptr;

  // The SurfaceFactoryClient and hierarchy can be registered/unregistered
  // in either order, so empty namespace_client_map entries need to be
  // checked when removing either clients or relationships.
  if (iter->second.is_empty())
    frame_sink_source_map_.erase(iter);
}

void SurfaceManager::RegisterBeginFrameSource(
    BeginFrameSource* source,
    const FrameSinkId& frame_sink_id) {
  DCHECK(source);
  DCHECK_EQ(registered_sources_.count(source), 0u);
  DCHECK_EQ(valid_frame_sink_ids_.count(frame_sink_id), 1u);

  registered_sources_[source] = frame_sink_id;
  RecursivelyAttachBeginFrameSource(frame_sink_id, source);
}

void SurfaceManager::UnregisterBeginFrameSource(BeginFrameSource* source) {
  DCHECK(source);
  DCHECK_EQ(registered_sources_.count(source), 1u);

  FrameSinkId frame_sink_id = registered_sources_[source];
  registered_sources_.erase(source);

  if (frame_sink_source_map_.count(frame_sink_id) == 0u)
    return;

  // TODO(enne): these walks could be done in one step.
  // Remove this begin frame source from its subtree.
  RecursivelyDetachBeginFrameSource(frame_sink_id, source);
  // Then flush every remaining registered source to fix any sources that
  // became null because of the previous step but that have an alternative.
  for (auto source_iter : registered_sources_)
    RecursivelyAttachBeginFrameSource(source_iter.second, source_iter.first);
}

void SurfaceManager::RecursivelyAttachBeginFrameSource(
    const FrameSinkId& frame_sink_id,
    BeginFrameSource* source) {
  FrameSinkSourceMapping& mapping = frame_sink_source_map_[frame_sink_id];
  if (!mapping.source) {
    mapping.source = source;
    if (mapping.client)
      mapping.client->SetBeginFrameSource(source);
  }
  for (size_t i = 0; i < mapping.children.size(); ++i)
    RecursivelyAttachBeginFrameSource(mapping.children[i], source);
}

void SurfaceManager::RecursivelyDetachBeginFrameSource(
    const FrameSinkId& frame_sink_id,
    BeginFrameSource* source) {
  auto iter = frame_sink_source_map_.find(frame_sink_id);
  if (iter == frame_sink_source_map_.end())
    return;
  if (iter->second.source == source) {
    iter->second.source = nullptr;
    if (iter->second.client)
      iter->second.client->SetBeginFrameSource(nullptr);
  }

  if (iter->second.is_empty()) {
    frame_sink_source_map_.erase(iter);
    return;
  }

  std::vector<FrameSinkId>& children = iter->second.children;
  for (size_t i = 0; i < children.size(); ++i) {
    RecursivelyDetachBeginFrameSource(children[i], source);
  }
}

bool SurfaceManager::ChildContains(
    const FrameSinkId& child_frame_sink_id,
    const FrameSinkId& search_frame_sink_id) const {
  auto iter = frame_sink_source_map_.find(child_frame_sink_id);
  if (iter == frame_sink_source_map_.end())
    return false;

  const std::vector<FrameSinkId>& children = iter->second.children;
  for (size_t i = 0; i < children.size(); ++i) {
    if (children[i] == search_frame_sink_id)
      return true;
    if (ChildContains(children[i], search_frame_sink_id))
      return true;
  }
  return false;
}

void SurfaceManager::RegisterFrameSinkHierarchy(
    const FrameSinkId& parent_frame_sink_id,
    const FrameSinkId& child_frame_sink_id) {
  DCHECK_EQ(valid_frame_sink_ids_.count(parent_frame_sink_id), 1u);
  DCHECK_EQ(valid_frame_sink_ids_.count(child_frame_sink_id), 1u);

  // If it's possible to reach the parent through the child's descendant chain,
  // then this will create an infinite loop.  Might as well just crash here.
  CHECK(!ChildContains(child_frame_sink_id, parent_frame_sink_id));

  std::vector<FrameSinkId>& children =
      frame_sink_source_map_[parent_frame_sink_id].children;
  for (size_t i = 0; i < children.size(); ++i)
    DCHECK(children[i] != child_frame_sink_id);
  children.push_back(child_frame_sink_id);

  // If the parent has no source, then attaching it to this child will
  // not change any downstream sources.
  BeginFrameSource* parent_source =
      frame_sink_source_map_[parent_frame_sink_id].source;
  if (!parent_source)
    return;

  DCHECK_EQ(registered_sources_.count(parent_source), 1u);
  RecursivelyAttachBeginFrameSource(child_frame_sink_id, parent_source);
}

void SurfaceManager::UnregisterFrameSinkHierarchy(
    const FrameSinkId& parent_frame_sink_id,
    const FrameSinkId& child_frame_sink_id) {
  // Deliberately do not check validity of either parent or child namespace
  // here.  They were valid during the registration, so were valid at some
  // point in time.  This makes it possible to invalidate parent and child
  // namespaces independently of each other and not have an ordering dependency
  // of unregistering the hierarchy first before either of them.
  DCHECK_EQ(frame_sink_source_map_.count(parent_frame_sink_id), 1u);

  auto iter = frame_sink_source_map_.find(parent_frame_sink_id);

  std::vector<FrameSinkId>& children = iter->second.children;
  bool found_child = false;
  for (size_t i = 0; i < children.size(); ++i) {
    if (children[i] == child_frame_sink_id) {
      found_child = true;
      children[i] = children.back();
      children.resize(children.size() - 1);
      break;
    }
  }
  DCHECK(found_child);

  // The SurfaceFactoryClient and hierarchy can be registered/unregistered
  // in either order, so empty namespace_client_map entries need to be
  // checked when removing either clients or relationships.
  if (iter->second.is_empty()) {
    frame_sink_source_map_.erase(iter);
    return;
  }

  // If the parent does not have a begin frame source, then disconnecting it
  // will not change any of its children.
  BeginFrameSource* parent_source = iter->second.source;
  if (!parent_source)
    return;

  // TODO(enne): these walks could be done in one step.
  RecursivelyDetachBeginFrameSource(child_frame_sink_id, parent_source);
  for (auto source_iter : registered_sources_)
    RecursivelyAttachBeginFrameSource(source_iter.second, source_iter.first);
}

Surface* SurfaceManager::GetSurfaceForId(const SurfaceId& surface_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  SurfaceMap::iterator it = surface_map_.find(surface_id);
  if (it == surface_map_.end())
    return NULL;
  return it->second;
}

bool SurfaceManager::SurfaceModified(const SurfaceId& surface_id) {
  CHECK(thread_checker_.CalledOnValidThread());
  bool changed = false;
  FOR_EACH_OBSERVER(SurfaceDamageObserver, observer_list_,
                    OnSurfaceDamaged(surface_id, &changed));
  return changed;
}

}  // namespace cc
