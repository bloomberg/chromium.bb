// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/surface_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <queue>
#include <utility>

#include "base/logging.h"
#include "cc/surfaces/direct_surface_reference_factory.h"
#include "cc/surfaces/stub_surface_reference_factory.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_client.h"
#include "components/viz/common/surfaces/local_surface_id_allocator.h"
#include "components/viz/common/surfaces/surface_info.h"

#if DCHECK_IS_ON()
#include <sstream>
#endif

namespace cc {

SurfaceManager::SurfaceReferenceInfo::SurfaceReferenceInfo() = default;

SurfaceManager::SurfaceReferenceInfo::~SurfaceReferenceInfo() = default;

SurfaceManager::SurfaceManager(LifetimeType lifetime_type)
    : lifetime_type_(lifetime_type),
      dependency_tracker_(this),
      root_surface_id_(
          viz::FrameSinkId(0u, 0u),
          viz::LocalSurfaceId(1u, base::UnguessableToken::Create())),
      weak_factory_(this) {
  thread_checker_.DetachFromThread();
  if (using_surface_references()) {
    reference_factory_ = new StubSurfaceReferenceFactory();
  } else {
    reference_factory_ =
        new DirectSurfaceReferenceFactory(weak_factory_.GetWeakPtr());
  }
}

SurfaceManager::~SurfaceManager() {
  // All SurfaceClients and their surfaces are supposed to be
  // destroyed before SurfaceManager.
  DCHECK_EQ(surfaces_to_destroy_.size(), surface_map_.size());
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

void SurfaceManager::RequestSurfaceResolution(Surface* surface) {
  dependency_tracker_.RequestSurfaceResolution(surface);
}

Surface* SurfaceManager::CreateSurface(
    base::WeakPtr<SurfaceClient> surface_client,
    const viz::SurfaceInfo& surface_info,
    BeginFrameSource* begin_frame_source,
    bool needs_sync_tokens) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(surface_info.is_valid());
  DCHECK(surface_client);

  // If no surface with this viz::SurfaceId exists, simply create the surface
  // and return.
  auto it = surface_map_.find(surface_info.id());
  if (it == surface_map_.end()) {
    surface_map_[surface_info.id()] =
        base::MakeUnique<Surface>(surface_info, this, surface_client,
                                  begin_frame_source, needs_sync_tokens);
    return surface_map_[surface_info.id()].get();
  }

  // If a surface with this viz::SurfaceId exists, it must be marked as
  // destroyed. Otherwise, we wouldn't receive a request to reuse the same
  // viz::SurfaceId. Remove the surface out of the garbage collector's queue and
  // reuse it.
  Surface* surface = it->second.get();
  DCHECK(IsMarkedForDestruction(surface_info.id()));
  surfaces_to_destroy_.erase(surface_info.id());
  return surface;
}

void SurfaceManager::DestroySurface(const viz::SurfaceId& surface_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(surface_map_.count(surface_id));
  for (auto& observer : observer_list_)
    observer.OnSurfaceDestroyed(surface_id);
  surfaces_to_destroy_.insert(surface_id);
  GarbageCollectSurfaces();
}

void SurfaceManager::SurfaceWillDraw(const viz::SurfaceId& surface_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (auto& observer : observer_list_)
    observer.OnSurfaceWillDraw(surface_id);
}

void SurfaceManager::RequireSequence(const viz::SurfaceId& surface_id,
                                     const viz::SurfaceSequence& sequence) {
  auto* surface = GetSurfaceForId(surface_id);
  if (!surface) {
    DLOG(ERROR) << "Attempting to require callback on nonexistent surface";
    return;
  }
  surface->AddDestructionDependency(sequence);
}

void SurfaceManager::SatisfySequence(const viz::SurfaceSequence& sequence) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(lifetime_type_, LifetimeType::SEQUENCES);
  satisfied_sequences_.insert(sequence);
  GarbageCollectSurfaces();
}

void SurfaceManager::RegisterFrameSinkId(
    const viz::FrameSinkId& frame_sink_id) {
  bool inserted = valid_frame_sink_ids_.insert(frame_sink_id).second;
  DCHECK(inserted);
}

void SurfaceManager::InvalidateFrameSinkId(
    const viz::FrameSinkId& frame_sink_id) {
  valid_frame_sink_ids_.erase(frame_sink_id);

  // Remove any temporary references owned by |frame_sink_id|.
  std::vector<viz::SurfaceId> temp_refs_to_clear;
  for (auto& map_entry : temporary_references_) {
    base::Optional<viz::FrameSinkId>& owner = map_entry.second;
    if (owner.has_value() && owner.value() == frame_sink_id)
      temp_refs_to_clear.push_back(map_entry.first);
  }

  for (auto& surface_id : temp_refs_to_clear)
    RemoveTemporaryReference(surface_id, false);

  GarbageCollectSurfaces();
}

const viz::SurfaceId& SurfaceManager::GetRootSurfaceId() const {
  return root_surface_id_;
}

void SurfaceManager::AddSurfaceReferences(
    const std::vector<SurfaceReference>& references) {
  DCHECK(thread_checker_.CalledOnValidThread());

  for (const auto& reference : references)
    AddSurfaceReferenceImpl(reference.parent_id(), reference.child_id());
}

void SurfaceManager::RemoveSurfaceReferences(
    const std::vector<SurfaceReference>& references) {
  DCHECK(thread_checker_.CalledOnValidThread());

  for (const auto& reference : references)
    RemoveSurfaceReferenceImpl(reference.parent_id(), reference.child_id());

  GarbageCollectSurfaces();
}

void SurfaceManager::AssignTemporaryReference(const viz::SurfaceId& surface_id,
                                              const viz::FrameSinkId& owner) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(lifetime_type_, LifetimeType::REFERENCES);

  if (!HasTemporaryReference(surface_id))
    return;

  temporary_references_[surface_id] = owner;
}

void SurfaceManager::DropTemporaryReference(const viz::SurfaceId& surface_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(lifetime_type_, LifetimeType::REFERENCES);

  if (!HasTemporaryReference(surface_id))
    return;

  RemoveTemporaryReference(surface_id, false);
}

const base::flat_set<viz::SurfaceId>&
SurfaceManager::GetSurfacesReferencedByParent(
    const viz::SurfaceId& surface_id) const {
  auto iter = references_.find(surface_id);
  if (iter == references_.end())
    return empty_surface_id_set_;
  return iter->second.children;
}

const base::flat_set<viz::SurfaceId>&
SurfaceManager::GetSurfacesThatReferenceChild(
    const viz::SurfaceId& surface_id) const {
  auto iter = references_.find(surface_id);
  if (iter == references_.end())
    return empty_surface_id_set_;
  return iter->second.parents;
}

void SurfaceManager::GarbageCollectSurfaces() {
  if (surfaces_to_destroy_.empty())
    return;

  SurfaceIdSet reachable_surfaces = using_surface_references()
                                        ? GetLiveSurfacesForReferences()
                                        : GetLiveSurfacesForSequences();

  std::vector<viz::SurfaceId> surfaces_to_delete;

  // Delete all destroyed and unreachable surfaces.
  for (auto iter = surfaces_to_destroy_.begin();
       iter != surfaces_to_destroy_.end();) {
    if (reachable_surfaces.count(*iter) == 0) {
      surfaces_to_delete.push_back(*iter);
      iter = surfaces_to_destroy_.erase(iter);
    } else {
      ++iter;
    }
  }

  // ~Surface() draw callback could modify |surfaces_to_destroy_|.
  for (const viz::SurfaceId& surface_id : surfaces_to_delete)
    DestroySurfaceInternal(surface_id);
}

SurfaceManager::SurfaceIdSet SurfaceManager::GetLiveSurfacesForReferences() {
  DCHECK(using_surface_references());

  SurfaceIdSet reachable_surfaces;

  // Walk down from the root and mark each viz::SurfaceId we encounter as
  // reachable.
  std::queue<viz::SurfaceId> surface_queue;
  surface_queue.push(root_surface_id_);

  // All temporary references are also reachable.
  for (auto& map_entry : temporary_references_) {
    reachable_surfaces.insert(map_entry.first);
    surface_queue.push(map_entry.first);
  }

  while (!surface_queue.empty()) {
    const auto& children = GetSurfacesReferencedByParent(surface_queue.front());
    for (const viz::SurfaceId& child_id : children) {
      // Check for cycles when inserting into |reachable_surfaces|.
      if (reachable_surfaces.insert(child_id).second)
        surface_queue.push(child_id);
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
  std::vector<viz::SurfaceId> live_surfaces;
  std::unordered_set<viz::SurfaceId, viz::SurfaceIdHash> live_surfaces_set;

  // GC roots are surfaces that have not been destroyed, or have not had all
  // their destruction dependencies satisfied.
  for (auto& map_entry : surface_map_) {
    const viz::SurfaceId& surface_id = map_entry.first;
    Surface* surface = map_entry.second.get();
    surface->SatisfyDestructionDependencies(&satisfied_sequences_,
                                            &valid_frame_sink_ids_);

    if (!IsMarkedForDestruction(surface_id) ||
        surface->GetDestructionDependencyCount() > 0) {
      live_surfaces_set.insert(surface_id);
      live_surfaces.push_back(surface_id);
    }
  }

  // Mark all surfaces reachable from live surfaces by adding them to
  // live_surfaces and live_surfaces_set.
  for (size_t i = 0; i < live_surfaces.size(); i++) {
    Surface* surf = surface_map_[live_surfaces[i]].get();
    DCHECK(surf);

    const auto& children = GetSurfacesReferencedByParent(surf->surface_id());
    for (const viz::SurfaceId& id : children) {
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

void SurfaceManager::AddSurfaceReferenceImpl(const viz::SurfaceId& parent_id,
                                             const viz::SurfaceId& child_id) {
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

  references_[parent_id].children.insert(child_id);
  references_[child_id].parents.insert(parent_id);

  if (HasTemporaryReference(child_id))
    RemoveTemporaryReference(child_id, true);
}

void SurfaceManager::RemoveSurfaceReferenceImpl(
    const viz::SurfaceId& parent_id,
    const viz::SurfaceId& child_id) {
  auto iter_parent = references_.find(parent_id);
  auto iter_child = references_.find(child_id);
  if (iter_parent == references_.end() || iter_child == references_.end())
    return;

  iter_parent->second.children.erase(child_id);
  iter_child->second.parents.erase(parent_id);
}

void SurfaceManager::RemoveAllSurfaceReferences(
    const viz::SurfaceId& surface_id) {
  DCHECK(!HasTemporaryReference(surface_id));

  auto iter = references_.find(surface_id);
  if (iter != references_.end()) {
    // Remove all references from |surface_id| to a child surface.
    for (const viz::SurfaceId& child_id : iter->second.children)
      references_[child_id].parents.erase(surface_id);

    // Remove all reference from parent surface to |surface_id|.
    for (const viz::SurfaceId& parent_id : iter->second.parents)
      references_[parent_id].children.erase(surface_id);

    references_.erase(iter);
  }
}

bool SurfaceManager::HasTemporaryReference(
    const viz::SurfaceId& surface_id) const {
  return temporary_references_.count(surface_id) != 0;
}

void SurfaceManager::AddTemporaryReference(const viz::SurfaceId& surface_id) {
  DCHECK(!HasTemporaryReference(surface_id));

  // Add an entry to |temporary_references_| with no owner for the temporary
  // reference. Also add a range tracking entry so we know the order that
  // surfaces were created for the viz::FrameSinkId.
  temporary_references_[surface_id] = base::Optional<viz::FrameSinkId>();
  temporary_reference_ranges_[surface_id.frame_sink_id()].push_back(
      surface_id.local_surface_id());
}

void SurfaceManager::RemoveTemporaryReference(const viz::SurfaceId& surface_id,
                                              bool remove_range) {
  DCHECK(HasTemporaryReference(surface_id));

  const viz::FrameSinkId& frame_sink_id = surface_id.frame_sink_id();
  std::vector<viz::LocalSurfaceId>& frame_sink_temp_refs =
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
    temporary_references_.erase(viz::SurfaceId(frame_sink_id, *iter));
  frame_sink_temp_refs.erase(begin_iter, end_iter);

  // If last temporary reference is removed for |frame_sink_id| then cleanup
  // range tracking map entry.
  if (frame_sink_temp_refs.empty())
    temporary_reference_ranges_.erase(frame_sink_id);
}

Surface* SurfaceManager::GetSurfaceForId(const viz::SurfaceId& surface_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  auto it = surface_map_.find(surface_id);
  if (it == surface_map_.end())
    return nullptr;
  return it->second.get();
}

bool SurfaceManager::SurfaceModified(const viz::SurfaceId& surface_id,
                                     const BeginFrameAck& ack) {
  CHECK(thread_checker_.CalledOnValidThread());
  bool changed = false;
  for (auto& observer : observer_list_)
    changed |= observer.OnSurfaceDamaged(surface_id, ack);
  return changed;
}

void SurfaceManager::SurfaceCreated(const viz::SurfaceInfo& surface_info) {
  CHECK(thread_checker_.CalledOnValidThread());

  if (lifetime_type_ == LifetimeType::REFERENCES) {
    // We can get into a situation where multiple CompositorFrames arrive for
    // a FrameSink before the client can add any references for the frame. When
    // the second frame with a new size arrives, the first will be destroyed in
    // SurfaceFactory and then if there are no references it will be deleted
    // during surface GC. A temporary reference, removed when a real reference
    // is received, is added to prevent this from happening.
    AddTemporaryReference(surface_info.id());
  }

  for (auto& observer : observer_list_)
    observer.OnSurfaceCreated(surface_info);
}

void SurfaceManager::SurfaceActivated(Surface* surface) {
  dependency_tracker_.OnSurfaceActivated(surface);
}

void SurfaceManager::SurfaceDependenciesChanged(
    Surface* surface,
    const base::flat_set<viz::SurfaceId>& added_dependencies,
    const base::flat_set<viz::SurfaceId>& removed_dependencies) {
  dependency_tracker_.OnSurfaceDependenciesChanged(surface, added_dependencies,
                                                   removed_dependencies);
}

void SurfaceManager::SurfaceDiscarded(Surface* surface) {
  for (auto& observer : observer_list_)
    observer.OnSurfaceDiscarded(surface->surface_id());
  dependency_tracker_.OnSurfaceDiscarded(surface);
}

void SurfaceManager::SurfaceDamageExpected(const viz::SurfaceId& surface_id,
                                           const BeginFrameArgs& args) {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (auto& observer : observer_list_)
    observer.OnSurfaceDamageExpected(surface_id, args);
}

void SurfaceManager::DestroySurfaceInternal(const viz::SurfaceId& surface_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  auto it = surface_map_.find(surface_id);
  DCHECK(it != surface_map_.end());
  // Make sure that the surface is removed from the map before being actually
  // destroyed. An ack could be sent during the destruction of a surface which
  // could trigger a synchronous frame submission to a half-destroyed surface
  // and that's not desirable.
  std::unique_ptr<Surface> doomed = std::move(it->second);
  surface_map_.erase(it);
  RemoveAllSurfaceReferences(surface_id);
}

#if DCHECK_IS_ON()
void SurfaceManager::SurfaceReferencesToStringImpl(
    const viz::SurfaceId& surface_id,
    std::string indent,
    std::stringstream* str) {
  *str << indent;

  // Print the current line for |surface_id|.
  Surface* surface = GetSurfaceForId(surface_id);
  if (surface) {
    *str << surface->surface_id().ToString();
    *str << (IsMarkedForDestruction(surface_id) ? " destroyed" : " live");

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
  for (const viz::SurfaceId& child_id :
       GetSurfacesReferencedByParent(surface_id))
    SurfaceReferencesToStringImpl(child_id, indent + "  ", str);
}
#endif  // DCHECK_IS_ON()

bool SurfaceManager::IsMarkedForDestruction(const viz::SurfaceId& surface_id) {
  return surfaces_to_destroy_.count(surface_id) != 0;
}

}  // namespace cc
