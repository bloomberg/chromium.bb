// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/surface_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <queue>
#include <utility>

#include "base/logging.h"
#include "cc/surfaces/compositor_frame_sink_support.h"
#include "cc/surfaces/direct_surface_reference_factory.h"
#include "cc/surfaces/local_surface_id_allocator.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_info.h"

#if DCHECK_IS_ON()
#include <sstream>
#endif

namespace cc {

SurfaceManager::SurfaceManager(LifetimeType lifetime_type)
    : lifetime_type_(lifetime_type),
      root_surface_id_(FrameSinkId(0u, 0u),
                       LocalSurfaceId(1u, base::UnguessableToken::Create())),
      weak_factory_(this) {
  thread_checker_.DetachFromThread();
  reference_factory_ =
      new DirectSurfaceReferenceFactory(weak_factory_.GetWeakPtr());
}

SurfaceManager::~SurfaceManager() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (using_surface_references()) {
    // Remove all temporary references on shutdown.
    temporary_references_.clear();
    temporary_reference_ranges_.clear();

    GarbageCollectSurfaces();
  }

  for (SurfaceDestroyList::iterator it = surfaces_to_destroy_.begin();
       it != surfaces_to_destroy_.end();
       ++it) {
    UnregisterSurface((*it)->surface_id());
  }
  surfaces_to_destroy_.clear();
}

#if DCHECK_IS_ON()
std::string SurfaceManager::SurfaceReferencesToString() {
  std::stringstream str;
  SurfaceReferencesToStringImpl(root_surface_id_, "", &str);
  // Temporary references will have an asterisk in front of them.
  for (auto& map_entry : temporary_references_)
    SurfaceReferencesToStringImpl(map_entry.first, "* ", &str);

  return str.str();
}
#endif

void SurfaceManager::SetDependencyTracker(
    std::unique_ptr<SurfaceDependencyTracker> dependency_tracker) {
  dependency_tracker_ = std::move(dependency_tracker);
}

void SurfaceManager::RequestSurfaceResolution(Surface* pending_surface) {
  if (dependency_tracker_)
    dependency_tracker_->RequestSurfaceResolution(pending_surface);
}

std::unique_ptr<Surface> SurfaceManager::CreateSurface(
    base::WeakPtr<CompositorFrameSinkSupport> compositor_frame_sink_support,
    const LocalSurfaceId& local_surface_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(local_surface_id.is_valid() && compositor_frame_sink_support);

  SurfaceId surface_id(compositor_frame_sink_support->frame_sink_id(),
                       local_surface_id);

  // If no surface with this SurfaceId exists, simply create the surface and
  // return.
  auto surface_iter = surface_map_.find(surface_id);
  if (surface_iter == surface_map_.end()) {
    auto surface =
        base::MakeUnique<Surface>(surface_id, compositor_frame_sink_support);
    surface_map_[surface->surface_id()] = surface.get();
    return surface;
  }

  // If a surface with this SurfaceId exists and it's not marked as destroyed,
  // we should not receive a request to create a new surface with the same
  // SurfaceId.
  DCHECK(surface_iter->second->destroyed());

  // If a surface with this SurfaceId exists and it's marked as destroyed,
  // it means it's in the garbage collector's queue. We simply take it out of
  // the queue and reuse it.
  auto it =
      std::find_if(surfaces_to_destroy_.begin(), surfaces_to_destroy_.end(),
                   [&surface_id](const std::unique_ptr<Surface>& surface) {
                     return surface->surface_id() == surface_id;
                   });
  DCHECK(it != surfaces_to_destroy_.end());
  std::unique_ptr<Surface> surface = std::move(*it);
  surfaces_to_destroy_.erase(it);
  surface->set_destroyed(false);
  DCHECK_EQ(compositor_frame_sink_support.get(),
            surface->compositor_frame_sink_support().get());
  return surface;
}

void SurfaceManager::DestroySurface(std::unique_ptr<Surface> surface) {
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
  framesink_manager_.RegisterFrameSinkId(frame_sink_id);
}

void SurfaceManager::InvalidateFrameSinkId(const FrameSinkId& frame_sink_id) {
  framesink_manager_.InvalidateFrameSinkId(frame_sink_id);

  if (using_surface_references()) {
    // Remove any temporary references owned by |frame_sink_id|.
    std::vector<SurfaceId> temp_refs_to_clear;
    for (auto& map_entry : temporary_references_) {
      base::Optional<FrameSinkId>& owner = map_entry.second;
      if (owner.has_value() && owner.value() == frame_sink_id)
        temp_refs_to_clear.push_back(map_entry.first);
    }

    for (auto& surface_id : temp_refs_to_clear)
      RemoveTemporaryReference(surface_id, false);
  }

  GarbageCollectSurfaces();
}

const SurfaceId& SurfaceManager::GetRootSurfaceId() const {
  return root_surface_id_;
}

void SurfaceManager::AddSurfaceReferences(
    const std::vector<SurfaceReference>& references) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(using_surface_references());

  for (const auto& reference : references)
    AddSurfaceReferenceImpl(reference.parent_id(), reference.child_id());
}

void SurfaceManager::RemoveSurfaceReferences(
    const std::vector<SurfaceReference>& references) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(using_surface_references());

  for (const auto& reference : references)
    RemoveSurfaceReferenceImpl(reference.parent_id(), reference.child_id());

  GarbageCollectSurfaces();
}

void SurfaceManager::AssignTemporaryReference(const SurfaceId& surface_id,
                                              const FrameSinkId& owner) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(lifetime_type_, LifetimeType::REFERENCES);

  if (!HasTemporaryReference(surface_id))
    return;

  temporary_references_[surface_id] = owner;
}

void SurfaceManager::DropTemporaryReference(const SurfaceId& surface_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(lifetime_type_, LifetimeType::REFERENCES);

  if (!HasTemporaryReference(surface_id))
    return;

  RemoveTemporaryReference(surface_id, false);
}

void SurfaceManager::GarbageCollectSurfaces() {
  if (surfaces_to_destroy_.empty())
    return;

  SurfaceIdSet reachable_surfaces = using_surface_references()
                                        ? GetLiveSurfacesForReferences()
                                        : GetLiveSurfacesForSequences();

  std::vector<std::unique_ptr<Surface>> surfaces_to_delete;

  // Delete all destroyed and unreachable surfaces.
  for (auto iter = surfaces_to_destroy_.begin();
       iter != surfaces_to_destroy_.end();) {
    SurfaceId surface_id = (*iter)->surface_id();
    if (reachable_surfaces.count(surface_id) == 0) {
      UnregisterSurface(surface_id);
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
  DCHECK(using_surface_references());

  SurfaceIdSet reachable_surfaces;

  // Walk down from the root and mark each SurfaceId we encounter as reachable.
  std::queue<SurfaceId> surface_queue;
  surface_queue.push(root_surface_id_);

  // All temporary references are also reachable.
  for (auto& map_entry : temporary_references_) {
    reachable_surfaces.insert(map_entry.first);
    surface_queue.push(map_entry.first);
  }

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
                                  framesink_manager_.GetValidFrameSinkIds());

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

    // TODO(fsamuel): We should probably keep alive pending referenced surfaces
    // too.
    if (!surf->active_referenced_surfaces())
      continue;

    for (const SurfaceId& id : *surf->active_referenced_surfaces()) {
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

  parent_to_child_refs_[parent_id].insert(child_id);
  child_to_parent_refs_[child_id].insert(parent_id);

  if (HasTemporaryReference(child_id))
    RemoveTemporaryReference(child_id, true);
}

void SurfaceManager::RemoveSurfaceReferenceImpl(const SurfaceId& parent_id,
                                                const SurfaceId& child_id) {
  if (parent_to_child_refs_.count(parent_id) == 0 ||
      parent_to_child_refs_[parent_id].count(child_id) == 0) {
    DLOG(ERROR) << "No reference from " << parent_id << " to " << child_id;
    return;
  }

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

bool SurfaceManager::HasTemporaryReference(const SurfaceId& surface_id) const {
  return temporary_references_.count(surface_id) != 0;
}

void SurfaceManager::AddTemporaryReference(const SurfaceId& surface_id) {
  DCHECK(!HasTemporaryReference(surface_id));

  // Add an entry to |temporary_references_| with no owner for the temporary
  // reference. Also add a range tracking entry so we know the order that
  // surfaces were created for the FrameSinkId.
  temporary_references_[surface_id] = base::Optional<FrameSinkId>();
  temporary_reference_ranges_[surface_id.frame_sink_id()].push_back(
      surface_id.local_surface_id());
}

void SurfaceManager::RemoveTemporaryReference(const SurfaceId& surface_id,
                                              bool remove_range) {
  DCHECK(HasTemporaryReference(surface_id));

  const FrameSinkId& frame_sink_id = surface_id.frame_sink_id();
  std::vector<LocalSurfaceId>& frame_sink_temp_refs =
      temporary_reference_ranges_[frame_sink_id];

  // Find the iterator to the range tracking entry for |surface_id|. Use that
  // iterator and |remove_range| to find the right begin and end iterators for
  // the temporary references we want to remove.
  auto surface_id_iter =
      std::find(frame_sink_temp_refs.begin(), frame_sink_temp_refs.end(),
                surface_id.local_surface_id());
  auto begin_iter =
      remove_range ? frame_sink_temp_refs.begin() : surface_id_iter;
  auto end_iter = surface_id_iter + 1;

  // Remove temporary references and range tracking information.
  for (auto iter = begin_iter; iter != end_iter; ++iter)
    temporary_references_.erase(SurfaceId(frame_sink_id, *iter));
  frame_sink_temp_refs.erase(begin_iter, end_iter);

  // If last temporary reference is removed for |frame_sink_id| then cleanup
  // range tracking map entry.
  if (frame_sink_temp_refs.empty())
    temporary_reference_ranges_.erase(frame_sink_id);
}

void SurfaceManager::RegisterFrameSinkManagerClient(
    const FrameSinkId& frame_sink_id,
    FrameSinkManagerClient* client) {
  framesink_manager_.RegisterFrameSinkManagerClient(frame_sink_id, client);
}

void SurfaceManager::UnregisterFrameSinkManagerClient(
    const FrameSinkId& frame_sink_id) {
  framesink_manager_.UnregisterFrameSinkManagerClient(frame_sink_id);
}

void SurfaceManager::RegisterBeginFrameSource(
    BeginFrameSource* source,
    const FrameSinkId& frame_sink_id) {
  framesink_manager_.RegisterBeginFrameSource(source, frame_sink_id);
}

void SurfaceManager::UnregisterBeginFrameSource(BeginFrameSource* source) {
  framesink_manager_.UnregisterBeginFrameSource(source);
}

void SurfaceManager::RegisterFrameSinkHierarchy(
    const FrameSinkId& parent_frame_sink_id,
    const FrameSinkId& child_frame_sink_id) {
  framesink_manager_.RegisterFrameSinkHierarchy(parent_frame_sink_id,
                                                child_frame_sink_id);
}

void SurfaceManager::UnregisterFrameSinkHierarchy(
    const FrameSinkId& parent_frame_sink_id,
    const FrameSinkId& child_frame_sink_id) {
  framesink_manager_.UnregisterFrameSinkHierarchy(parent_frame_sink_id,
                                                child_frame_sink_id);
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
    // We can get into a situation where multiple CompositorFrames arrive for
    // a CompositorFrameSink before the client can add any references for the
    // frame. When the second frame with a new size arrives, the first will be
    // destroyed in SurfaceFactory and then if there are no references it will
    // be deleted during surface GC. A temporary reference, removed when a
    // real reference is received, is added to prevent this from happening.
    AddTemporaryReference(surface_info.id());
  }

  for (auto& observer : observer_list_)
    observer.OnSurfaceCreated(surface_info);
}

void SurfaceManager::SurfaceActivated(Surface* surface) {
  if (dependency_tracker_)
    dependency_tracker_->OnSurfaceActivated(surface);
}

void SurfaceManager::SurfaceDependenciesChanged(
    Surface* surface,
    const base::flat_set<SurfaceId>& added_dependencies,
    const base::flat_set<SurfaceId>& removed_dependencies) {
  if (dependency_tracker_) {
    dependency_tracker_->OnSurfaceDependenciesChanged(
        surface, added_dependencies, removed_dependencies);
  }
}

void SurfaceManager::SurfaceDiscarded(Surface* surface) {
  if (dependency_tracker_)
    dependency_tracker_->OnSurfaceDiscarded(surface);
}

void SurfaceManager::UnregisterSurface(const SurfaceId& surface_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  SurfaceMap::iterator it = surface_map_.find(surface_id);
  DCHECK(it != surface_map_.end());
  surface_map_.erase(it);
  RemoveAllSurfaceReferences(surface_id);
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
    *str << (surface->destroyed() ? " destroyed" : " live");

    if (surface->HasPendingFrame()) {
      // This provides the surface size from the root render pass.
      const CompositorFrame& frame = surface->GetPendingFrame();
      *str << " pending "
           << frame.render_pass_list.back()->output_rect.size().ToString();
    }

    if (surface->HasActiveFrame()) {
      // This provides the surface size from the root render pass.
      const CompositorFrame& frame = surface->GetActiveFrame();
      *str << " active "
           << frame.render_pass_list.back()->output_rect.size().ToString();
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
