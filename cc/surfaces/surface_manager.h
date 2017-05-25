// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_SURFACE_MANAGER_H_
#define CC_SURFACES_SURFACE_MANAGER_H_

#include <stdint.h>

#include <list>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "cc/surfaces/frame_sink_id.h"
#include "cc/surfaces/frame_sink_manager.h"
#include "cc/surfaces/surface_dependency_tracker.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_observer.h"
#include "cc/surfaces/surface_reference.h"
#include "cc/surfaces/surface_reference_factory.h"
#include "cc/surfaces/surface_sequence.h"
#include "cc/surfaces/surfaces_export.h"

#if DCHECK_IS_ON()
#include <iosfwd>
#include <string>
#endif

namespace cc {
class BeginFrameSource;
class CompositorFrame;
class FrameSinkManagerClient;
class Surface;

namespace test {
class SurfaceSynchronizationTest;
}

class CC_SURFACES_EXPORT SurfaceManager {
 public:
  enum class LifetimeType {
    REFERENCES,
    SEQUENCES,
  };

  explicit SurfaceManager(LifetimeType lifetime_type = LifetimeType::SEQUENCES);
  ~SurfaceManager();

#if DCHECK_IS_ON()
  // Returns a string representation of all reachable surface references.
  std::string SurfaceReferencesToString();
#endif

  void SetDependencyTracker(SurfaceDependencyTracker* dependency_tracker);
  SurfaceDependencyTracker* dependency_tracker() { return dependency_tracker_; }

  void RequestSurfaceResolution(Surface* pending_surface);

  std::unique_ptr<Surface> CreateSurface(
      base::WeakPtr<CompositorFrameSinkSupport> compositor_frame_sink_support,
      const SurfaceInfo& surface_info);

  // Destroy the Surface once a set of sequence numbers has been satisfied.
  void DestroySurface(std::unique_ptr<Surface> surface);

  Surface* GetSurfaceForId(const SurfaceId& surface_id);

  void AddObserver(SurfaceObserver* obs) { observer_list_.AddObserver(obs); }

  void RemoveObserver(SurfaceObserver* obs) {
    observer_list_.RemoveObserver(obs);
  }

  bool SurfaceModified(const SurfaceId& surface_id);

  // Called when a CompositorFrame is submitted to a CompositorFrameSinkSupport
  // for a given |surface_id| for the first time.
  void SurfaceCreated(const SurfaceInfo& surface_info);

  // Called when a CompositorFrame within |surface| has activated.
  void SurfaceActivated(Surface* surface);

  // Called when the dependencies of a pending CompositorFrame within |surface|
  // has changed.
  void SurfaceDependenciesChanged(
      Surface* surface,
      const base::flat_set<SurfaceId>& added_dependencies,
      const base::flat_set<SurfaceId>& removed_dependencies);

  // Called when |surface| is being destroyed.
  void SurfaceDiscarded(Surface* surface);

  // Require that the given sequence number must be satisfied (using
  // SatisfySequence) before the given surface can be destroyed.
  void RequireSequence(const SurfaceId& surface_id,
                       const SurfaceSequence& sequence);

  // Satisfies the given sequence number. Once all sequence numbers that
  // a surface depends on are satisfied, the surface can be destroyed.
  void SatisfySequence(const SurfaceSequence& sequence);

  void RegisterFrameSinkId(const FrameSinkId& frame_sink_id);

  // Invalidate a frame_sink_id that might still have associated sequences,
  // possibly because a renderer process has crashed.
  void InvalidateFrameSinkId(const FrameSinkId& frame_sink_id);

  // CompositorFrameSinkSupport, hierarchy, and BeginFrameSource can be
  // registered and unregistered in any order with respect to each other.
  //
  // This happens in practice, e.g. the relationship to between ui::Compositor /
  // DelegatedFrameHost is known before ui::Compositor has a surface/client).
  // However, DelegatedFrameHost can register itself as a client before its
  // relationship with the ui::Compositor is known.

  // Associates a FrameSinkManagerClient with the surface id frame_sink_id it
  // uses.
  // FrameSinkManagerClient and surface namespaces/allocators have a 1:1
  // mapping. Caller guarantees the client is alive between register/unregister.
  // Reregistering the same namespace when a previous client is active is not
  // valid.
  void RegisterFrameSinkManagerClient(const FrameSinkId& frame_sink_id,
                                      FrameSinkManagerClient* client);
  void UnregisterFrameSinkManagerClient(const FrameSinkId& frame_sink_id);

  // Associates a |source| with a particular namespace.  That namespace and
  // any children of that namespace with valid clients can potentially use
  // that |source|.
  void RegisterBeginFrameSource(BeginFrameSource* source,
                                const FrameSinkId& frame_sink_id);
  void UnregisterBeginFrameSource(BeginFrameSource* source);

  // Returns a stable BeginFrameSource that forwards BeginFrames from the first
  // available BeginFrameSource.
  BeginFrameSource* GetPrimaryBeginFrameSource();

  // Register a relationship between two namespaces.  This relationship means
  // that surfaces from the child namespace will be displayed in the parent.
  // Children are allowed to use any begin frame source that their parent can
  // use.
  void RegisterFrameSinkHierarchy(const FrameSinkId& parent_frame_sink_id,
                                  const FrameSinkId& child_frame_sink_id);
  void UnregisterFrameSinkHierarchy(const FrameSinkId& parent_frame_sink_id,
                                    const FrameSinkId& child_frame_sink_id);

  // Returns the top level root SurfaceId. Surfaces that are not reachable
  // from the top level root may be garbage collected. It will not be a valid
  // SurfaceId and will never correspond to a surface.
  const SurfaceId& GetRootSurfaceId() const;

  // Adds all surface references in |references|. This will remove any temporary
  // references for child surface in a surface reference.
  void AddSurfaceReferences(const std::vector<SurfaceReference>& references);

  // Removes all surface references in |references| then runs garbage
  // collection to delete unreachable surfaces.
  void RemoveSurfaceReferences(const std::vector<SurfaceReference>& references);

  // Assigns |frame_sink_id| as the owner of the temporary reference to
  // |surface_id|. If |frame_sink_id| is invalidated the temporary reference
  // will be removed. If a surface reference has already been added from the
  // parent to |surface_id| then this will do nothing.
  void AssignTemporaryReference(const SurfaceId& surface_id,
                                const FrameSinkId& owner);

  // Drops the temporary reference for |surface_id|. If a surface reference has
  // already been added from the parent to |surface_id| then this will do
  // nothing.
  void DropTemporaryReference(const SurfaceId& surface_id);

  scoped_refptr<SurfaceReferenceFactory> reference_factory() {
    return reference_factory_;
  }

  bool using_surface_references() const {
    return lifetime_type_ == LifetimeType::REFERENCES;
  }

 private:
  friend class test::SurfaceSynchronizationTest;
  friend class SurfaceManagerRefTest;

  using SurfaceIdSet = std::unordered_set<SurfaceId, SurfaceIdHash>;

  // Garbage collects all destroyed surfaces that aren't live.
  void GarbageCollectSurfaces();

  // Returns set of live surfaces for |lifetime_manager_| is REFERENCES.
  SurfaceIdSet GetLiveSurfacesForReferences();

  // Returns set of live surfaces for |lifetime_manager_| is SEQUENCES.
  SurfaceIdSet GetLiveSurfacesForSequences();

  // Adds a reference from |parent_id| to |child_id| without dealing with
  // temporary references.
  void AddSurfaceReferenceImpl(const SurfaceId& parent_id,
                               const SurfaceId& child_id);

  // Removes a reference from a |parent_id| to |child_id|.
  void RemoveSurfaceReferenceImpl(const SurfaceId& parent_id,
                                  const SurfaceId& child_id);

  // Removes all surface references to or from |surface_id|. Used when the
  // surface is about to be deleted.
  void RemoveAllSurfaceReferences(const SurfaceId& surface_id);

  bool HasTemporaryReference(const SurfaceId& surface_id) const;

  // Adds a temporary reference to |surface_id|. The reference will not have an
  // owner initially.
  void AddTemporaryReference(const SurfaceId& surface_id);

  // Removes temporary reference to |surface_id|. If |remove_range| is true then
  // all temporary references to surfaces with the same FrameSinkId as
  // |surface_id| that were added before |surface_id| will also be removed.
  void RemoveTemporaryReference(const SurfaceId& surface_id, bool remove_range);

  // Called when a surface is destroyed and it needs to be removed from the
  // surface map.
  void UnregisterSurface(const SurfaceId& surface_id);

#if DCHECK_IS_ON()
  // Recursively prints surface references starting at |surface_id| to |str|.
  void SurfaceReferencesToStringImpl(const SurfaceId& surface_id,
                                     std::string indent,
                                     std::stringstream* str);
#endif

  // Use reference or sequence based lifetime management.
  LifetimeType lifetime_type_;

  FrameSinkManager framesink_manager_;

  using SurfaceMap = std::unordered_map<SurfaceId, Surface*, SurfaceIdHash>;
  SurfaceMap surface_map_;
  base::ObserverList<SurfaceObserver> observer_list_;
  base::ThreadChecker thread_checker_;

  // List of surfaces to be destroyed, along with what sequences they're still
  // waiting on.
  using SurfaceDestroyList = std::list<std::unique_ptr<Surface>>;
  SurfaceDestroyList surfaces_to_destroy_;

  // Set of SurfaceSequences that have been satisfied by a frame but not yet
  // waited on.
  std::unordered_set<SurfaceSequence, SurfaceSequenceHash> satisfied_sequences_;

  // Tracks references from the child surface to parent surface. If there are
  // zero entries in the set for a SurfaceId then nothing is referencing the
  // surface and it can be garbage collected.
  std::unordered_map<SurfaceId, base::flat_set<SurfaceId>, SurfaceIdHash>
      child_to_parent_refs_;
  // Tracks references from the parent surface to child surface. Is the inverse
  // of |child_to_parent_refs_|.
  std::unordered_map<SurfaceId, base::flat_set<SurfaceId>, SurfaceIdHash>
      parent_to_child_refs_;

  // Root SurfaceId that references display root surfaces. There is no Surface
  // with this id, it's for bookkeeping purposes only.
  const SurfaceId root_surface_id_;

  // The DirectSurfaceReferenceFactory that uses this manager to create surface
  // references.
  scoped_refptr<SurfaceReferenceFactory> reference_factory_;

  // A map of surfaces that have temporary references to them. The key is the
  // SurfaceId and the value is the owner. The owner will initially be empty and
  // set later by AssignTemporaryReference().
  std::unordered_map<SurfaceId, base::Optional<FrameSinkId>, SurfaceIdHash>
      temporary_references_;

  // Range tracking information for temporary references. Each map entry is an
  // is an ordered list of SurfaceIds that have temporary references with the
  // same FrameSinkId. A SurfaceId can be reconstructed with:
  //   SurfaceId surface_id(key, value[index]);
  // The LocalSurfaceIds are stored in the order the surfaces are created in. If
  // a reference is added to a later SurfaceId then all temporary references up
  // to that point will be removed. This is to handle clients getting out of
  // sync, for example the embedded client producing new SurfaceIds faster than
  // the embedding client can use them.
  std::unordered_map<FrameSinkId, std::vector<LocalSurfaceId>, FrameSinkIdHash>
      temporary_reference_ranges_;

  SurfaceDependencyTracker* dependency_tracker_ = nullptr;

  base::WeakPtrFactory<SurfaceManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceManager);
};

}  // namespace cc

#endif  // CC_SURFACES_SURFACE_MANAGER_H_
