// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/surface_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <queue>
#include <utility>

#include "base/logging.h"
#include "cc/surfaces/direct_surface_reference_factory.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_factory_client.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "cc/surfaces/surface_info.h"

#if DCHECK_IS_ON()
#include <sstream>
#endif

namespace cc {

SurfaceManager::FrameSinkSourceMapping::FrameSinkSourceMapping()
    : client(nullptr), source(nullptr) {}

SurfaceManager::FrameSinkSourceMapping::FrameSinkSourceMapping(
    const FrameSinkSourceMapping& other) = default;

SurfaceManager::FrameSinkSourceMapping::~FrameSinkSourceMapping() {
  DCHECK(is_empty()) << "client: " << client
                     << ", children: " << children.size();
}

SurfaceManager::SurfaceManager(LifetimeType lifetime_type)
    : lifetime_type_(lifetime_type),
      root_surface_id_(FrameSinkId(0u, 0u),
                       LocalFrameId(1u, base::UnguessableToken::Create())),
      weak_factory_(this) {
  thread_checker_.DetachFromThread();
  reference_factory_ =
      new DirectSurfaceReferenceFactory(weak_factory_.GetWeakPtr());
}

SurfaceManager::~SurfaceManager() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (lifetime_type_ == LifetimeType::REFERENCES) {
    // Remove all temporary references on shutdown.
    for (const auto& map_entry : temp_references_) {
      const FrameSinkId& frame_sink_id = map_entry.first;
      for (const auto& local_frame_id : map_entry.second) {
        RemoveSurfaceReferenceImpl(GetRootSurfaceId(),
                                   SurfaceId(frame_sink_id, local_frame_id));
      }
    }
    GarbageCollectSurfaces();
  }

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

#if DCHECK_IS_ON()
std::string SurfaceManager::SurfaceReferencesToString() {
  std::stringstream str;
  SurfaceReferencesToStringImpl(root_surface_id_, "", &str);
  return str.str();
}
#endif

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
  RemoveAllSurfaceReferences(surface_id);
}

void SurfaceManager::Destroy(std::unique_ptr<Surface> surface) {
  DCHECK(thread_checker_.CalledOnValidThread());
  surface->set_destroyed(true);
  surfaces_to_destroy_.push_back(std::move(surface));
  GarbageCollectSurfaces();
}

void SurfaceManager::RequireSequence(const SurfaceId& surface_id,
                                     const SurfaceSequence& sequence) {
  auto* surface = GetSurfaceForId(surface_id);
  if (!surface) {
    DLOG(ERROR) << "Attempting to require callback on nonexistent surface";
    return;
  }
  surface->AddDestructionDependency(sequence);
}

void SurfaceManager::SatisfySequence(const SurfaceSequence& sequence) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(lifetime_type_, LifetimeType::SEQUENCES);
  satisfied_sequences_.insert(sequence);
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

const SurfaceId& SurfaceManager::GetRootSurfaceId() const {
  return root_surface_id_;
}

void SurfaceManager::AddSurfaceReference(const SurfaceId& parent_id,
                                         const SurfaceId& child_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(lifetime_type_, LifetimeType::REFERENCES);

  // Check some conditions that should never happen. We don't want to crash on
  // bad input from a compromised client so just return early.
  if (parent_id.frame_sink_id() == child_id.frame_sink_id()) {
    DLOG(ERROR) << "Cannot add self reference from " << parent_id << " to "
                << child_id;
    return;
  }

  // We trust that |parent_id| either exists or is about to exist, since is not
  // sent over IPC. We don't trust |child_id|, since it is sent over IPC.
  if (surface_map_.count(child_id) == 0) {
    DLOG(ERROR) << "No surface in map for " << child_id.ToString();
    return;
  }

  // There could be a temporary reference to |child_id| which we should now
  // remove because a real reference is being added to it. To find out whether
  // or not a temporary reference exists, we need to first look up the
  // FrameSinkId of |child_id| in |temp_references_|, which returns a vector of
  // LocalFrameIds, and then search for the LocalFrameId of |child_id| in the
  // said vector. If there is no temporary reference, we can immediately add the
  // reference from |parent_id| and return.
  auto refs_iter = temp_references_.find(child_id.frame_sink_id());
  if (refs_iter == temp_references_.end()) {
    AddSurfaceReferenceImpl(parent_id, child_id);
    return;
  }
  std::vector<LocalFrameId>& refs = refs_iter->second;
  auto temp_ref_iter =
      std::find(refs.begin(), refs.end(), child_id.local_frame_id());
  if (temp_ref_iter == refs.end()) {
    AddSurfaceReferenceImpl(parent_id, child_id);
    return;
  }

  // Temporary references are implemented by holding a reference from the top
  // level root to the child. If |parent_id| is the top level root, we do
  // nothing because the reference already exists. Otherwise, remove the
  // temporary reference and add the reference.
  if (parent_id != GetRootSurfaceId()) {
    AddSurfaceReferenceImpl(parent_id, child_id);
    RemoveSurfaceReference(GetRootSurfaceId(), child_id);
  }

  // Remove temporary references for surfaces with the same FrameSinkId that
  // were created before |child_id|. The earlier surfaces were never embedded in
  // the parent and the parent is embedding a later surface, so we know the
  // parent doesn't need them anymore.
  for (auto iter = refs.begin(); iter != temp_ref_iter; ++iter) {
    SurfaceId id = SurfaceId(child_id.frame_sink_id(), *iter);
    RemoveSurfaceReference(GetRootSurfaceId(), id);
  }

  // Remove markers for temporary references up to |child_id|, as the temporary
  // references they correspond to were removed above. If |temp_ref_iter| points
  // at the last element in |refs| then we are removing all temporary references
  // for the FrameSinkId and can remove the map entry entirely.
  if (++temp_ref_iter == refs.end())
    temp_references_.erase(child_id.frame_sink_id());
  else
    refs.erase(refs.begin(), temp_ref_iter);
}

void SurfaceManager::RemoveSurfaceReference(const SurfaceId& parent_id,
                                            const SurfaceId& child_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(lifetime_type_, LifetimeType::REFERENCES);

  // Check if we have the reference that is requested to be removed. We don't
  // want to crash on bad input from a compromised client so just return early.
  if (parent_to_child_refs_.count(parent_id) == 0 ||
      parent_to_child_refs_[parent_id].count(child_id) == 0) {
    DLOG(ERROR) << "No reference from " << parent_id.ToString() << " to "
                << child_id.ToString();
    return;
  }

  RemoveSurfaceReferenceImpl(parent_id, child_id);
  GarbageCollectSurfaces();
}

size_t SurfaceManager::GetSurfaceReferenceCount(
    const SurfaceId& surface_id) const {
  auto iter = child_to_parent_refs_.find(surface_id);
  if (iter == child_to_parent_refs_.end())
    return 0;
  return iter->second.size();
}

size_t SurfaceManager::GetReferencedSurfaceCount(
    const SurfaceId& surface_id) const {
  auto iter = parent_to_child_refs_.find(surface_id);
  if (iter == parent_to_child_refs_.end())
    return 0;
  return iter->second.size();
}

void SurfaceManager::GarbageCollectSurfaces() {
  if (surfaces_to_destroy_.empty())
    return;

  SurfaceIdSet reachable_surfaces = lifetime_type_ == LifetimeType::REFERENCES
                                        ? GetLiveSurfacesForReferences()
                                        : GetLiveSurfacesForSequences();

  std::vector<std::unique_ptr<Surface>> surfaces_to_delete;

  // Delete all destroyed and unreachable surfaces.
  for (auto iter = surfaces_to_destroy_.begin();
       iter != surfaces_to_destroy_.end();) {
    SurfaceId surface_id = (*iter)->surface_id();
    if (reachable_surfaces.count(surface_id) == 0) {
      DeregisterSurface(surface_id);
      surfaces_to_delete.push_back(std::move(*iter));
      iter = surfaces_to_destroy_.erase(iter);
    } else {
      ++iter;
    }
  }

  // ~Surface() draw callback could modify |surfaces_to_destroy_|.
  surfaces_to_delete.clear();
}

SurfaceManager::SurfaceIdSet SurfaceManager::GetLiveSurfacesForReferences() {
  DCHECK_EQ(lifetime_type_, LifetimeType::REFERENCES);

  SurfaceIdSet reachable_surfaces;

  // Walk down from the root and mark each SurfaceId we encounter as reachable.
  std::queue<SurfaceId> surface_queue;
  surface_queue.push(root_surface_id_);
  while (!surface_queue.empty()) {
    const SurfaceId& surface_id = surface_queue.front();
    auto iter = parent_to_child_refs_.find(surface_id);
    if (iter != parent_to_child_refs_.end()) {
      for (const SurfaceId& child_id : iter->second) {
        // Check for cycles when inserting into |reachable_surfaces|.
        if (reachable_surfaces.insert(child_id).second)
          surface_queue.push(child_id);
      }
    }
    surface_queue.pop();
  }

  return reachable_surfaces;
}

SurfaceManager::SurfaceIdSet SurfaceManager::GetLiveSurfacesForSequences() {
  DCHECK_EQ(lifetime_type_, LifetimeType::SEQUENCES);

  // Simple mark and sweep GC.
  // TODO(jbauman): Reduce the amount of work when nothing needs to be
  // destroyed.
  std::vector<SurfaceId> live_surfaces;
  std::unordered_set<SurfaceId, SurfaceIdHash> live_surfaces_set;

  // GC roots are surfaces that have not been destroyed, or have not had all
  // their destruction dependencies satisfied.
  for (auto& map_entry : surface_map_) {
    const SurfaceId& surface_id = map_entry.first;
    Surface* surface = map_entry.second;
    surface->SatisfyDestructionDependencies(&satisfied_sequences_,
                                            &valid_frame_sink_ids_);

    if (!surface->destroyed() || surface->GetDestructionDependencyCount() > 0) {
      live_surfaces_set.insert(surface_id);
      live_surfaces.push_back(surface_id);
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

  return live_surfaces_set;
}

void SurfaceManager::AddSurfaceReferenceImpl(const SurfaceId& parent_id,
                                             const SurfaceId& child_id) {
  parent_to_child_refs_[parent_id].insert(child_id);
  child_to_parent_refs_[child_id].insert(parent_id);
}

void SurfaceManager::RemoveSurfaceReferenceImpl(const SurfaceId& parent_id,
                                                const SurfaceId& child_id) {
  parent_to_child_refs_[parent_id].erase(child_id);
  child_to_parent_refs_[child_id].erase(parent_id);
}

void SurfaceManager::RemoveAllSurfaceReferences(const SurfaceId& surface_id) {
  // Remove all references from |surface_id| to a child surface.
  auto iter = parent_to_child_refs_.find(surface_id);
  if (iter != parent_to_child_refs_.end()) {
    for (const SurfaceId& child_id : iter->second)
      child_to_parent_refs_[child_id].erase(surface_id);
    parent_to_child_refs_.erase(iter);
  }

  // Remove all reference from parent surface to |surface_id|.
  iter = child_to_parent_refs_.find(surface_id);
  if (iter != child_to_parent_refs_.end()) {
    for (const SurfaceId& parent_id : iter->second)
      parent_to_child_refs_[parent_id].erase(surface_id);
    child_to_parent_refs_.erase(iter);
  }
}

void SurfaceManager::RegisterSurfaceFactoryClient(
    const FrameSinkId& frame_sink_id,
    SurfaceFactoryClient* client) {
  DCHECK(client);
  DCHECK_EQ(valid_frame_sink_ids_.count(frame_sink_id), 1u);

  // Will create a new FrameSinkSourceMapping for |frame_sink_id| if necessary.
  FrameSinkSourceMapping& frame_sink_source =
      frame_sink_source_map_[frame_sink_id];
  DCHECK(!frame_sink_source.client);
  frame_sink_source.client = client;

  // Propagate any previously set sources to the new client.
  if (frame_sink_source.source)
    client->SetBeginFrameSource(frame_sink_source.source);
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
    return nullptr;
  return it->second;
}

bool SurfaceManager::SurfaceModified(const SurfaceId& surface_id) {
  CHECK(thread_checker_.CalledOnValidThread());
  bool changed = false;
  for (auto& observer : observer_list_)
    observer.OnSurfaceDamaged(surface_id, &changed);
  return changed;
}

void SurfaceManager::SurfaceCreated(const SurfaceInfo& surface_info) {
  CHECK(thread_checker_.CalledOnValidThread());

  if (lifetime_type_ == LifetimeType::REFERENCES) {
    // We can get into a situation where multiple CompositorFrames arrive for a
    // CompositorFrameSink before the client can add any references for the
    // frame. When the second frame with a new size arrives, the first will be
    // destroyed in SurfaceFactory and then if there are no references it will
    // be deleted during surface GC. A temporary reference, removed when a real
    // reference is received, is added to prevent this from happening.
    AddSurfaceReferenceImpl(GetRootSurfaceId(), surface_info.id());
    temp_references_[surface_info.id().frame_sink_id()].push_back(
        surface_info.id().local_frame_id());
  }

  for (auto& observer : observer_list_)
    observer.OnSurfaceCreated(surface_info);
}

#if DCHECK_IS_ON()
void SurfaceManager::SurfaceReferencesToStringImpl(const SurfaceId& surface_id,
                                                   std::string indent,
                                                   std::stringstream* str) {
  *str << indent;

  // Print the current line for |surface_id|.
  Surface* surface = GetSurfaceForId(surface_id);
  if (surface) {
    *str << surface->surface_id().ToString();
    *str << (surface->destroyed() ? " destroyed " : " live ");

    if (surface->HasFrame()) {
      // This provides the surface size from the root render pass.
      const CompositorFrame& frame = surface->GetEligibleFrame();
      *str << frame.render_pass_list.back()->output_rect.size().ToString();
    }
  } else {
    *str << surface_id;
  }
  *str << "\n";

  // If the current surface has references to children, sort children and print
  // references for each child.
  auto iter = parent_to_child_refs_.find(surface_id);
  if (iter != parent_to_child_refs_.end()) {
    std::vector<SurfaceId> children(iter->second.begin(), iter->second.end());
    std::sort(children.begin(), children.end());

    for (const SurfaceId& child_id : children)
      SurfaceReferencesToStringImpl(child_id, indent + "  ", str);
  }
}
#endif  // DCHECK_IS_ON()

}  // namespace cc
