// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_MANAGER_H_
#define COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_MANAGER_H_

#include <stdint.h>

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/service/surfaces/surface_dependency_tracker.h"
#include "components/viz/service/surfaces/surface_observer.h"
#include "components/viz/service/surfaces/surface_reference.h"

#if DCHECK_IS_ON()
#include <iosfwd>
#include <string>
#endif

namespace base {
class TickClock;
}  // namespace base

namespace viz {

namespace test {
class SurfaceReferencesTest;
class SurfaceSynchronizationTest;
}  // namespace test
class Surface;
struct BeginFrameAck;
struct BeginFrameArgs;

class VIZ_SERVICE_EXPORT SurfaceManager {
 public:
  explicit SurfaceManager(
      base::Optional<uint32_t> activation_deadline_in_frames);
  ~SurfaceManager();

#if DCHECK_IS_ON()
  // Returns a string representation of all reachable surface references.
  std::string SurfaceReferencesToString();
#endif

  // Sets an alternative system default frame activation deadline for unit
  // tests. base::nullopt indicates no deadline (in other words, an unlimited
  // deadline).
  void SetActivationDeadlineInFramesForTesting(
      base::Optional<uint32_t> deadline);

  base::Optional<uint32_t> activation_deadline_in_frames() const {
    return activation_deadline_in_frames_;
  }

  SurfaceDependencyTracker* dependency_tracker() {
    return &dependency_tracker_;
  }

  // Sets an alternative base::TickClock to pass into surfaces for surface
  // synchronization deadlines. This allows unit tests to mock the wall clock.
  void SetTickClockForTesting(base::TickClock* tick_clock);

  // Returns the base::TickClock used to set surface synchronization deadlines.
  base::TickClock* tick_clock() { return tick_clock_; }

  // Creates a Surface for the given SurfaceClient. The surface will be
  // destroyed when DestroySurface is called, all of its destruction
  // dependencies are satisfied, and it is not reachable from the root surface.
  // A temporary reference will be added to the new Surface.
  Surface* CreateSurface(base::WeakPtr<SurfaceClient> surface_client,
                         const SurfaceInfo& surface_info,
                         BeginFrameSource* begin_frame_source,
                         bool needs_sync_tokens);

  // Destroy the Surface once a set of sequence numbers has been satisfied.
  void DestroySurface(const SurfaceId& surface_id);

  Surface* GetSurfaceForId(const SurfaceId& surface_id);

  void AddObserver(SurfaceObserver* obs) { observer_list_.AddObserver(obs); }

  void RemoveObserver(SurfaceObserver* obs) {
    observer_list_.RemoveObserver(obs);
  }

  // Called when a Surface is modified, e.g. when a CompositorFrame is
  // activated, its producer confirms that no CompositorFrame will be submitted
  // in response to a BeginFrame, or a CopyOutputRequest is issued.
  //
  // |ack.sequence_number| is only valid if called in response to a BeginFrame.
  bool SurfaceModified(const SurfaceId& surface_id, const BeginFrameAck& ack);

  // Called when a surface has an active frame for the first time.
  void FirstSurfaceActivation(const SurfaceInfo& surface_info);

  // Called when a CompositorFrame within |surface| has activated. |duration| is
  // a measure of the time the frame has spent waiting on dependencies to
  // arrive. If |duration| is base::nullopt, then that indicates that this frame
  // was not blocked on dependencies.
  void SurfaceActivated(Surface* surface,
                        base::Optional<base::TimeDelta> duration);

  // Called when the dependencies of a pending CompositorFrame within |surface|
  // has changed.
  void SurfaceDependenciesChanged(
      Surface* surface,
      const base::flat_set<FrameSinkId>& added_dependencies,
      const base::flat_set<FrameSinkId>& removed_dependencies);

  // Called when |surface| is being destroyed.
  void SurfaceDiscarded(Surface* surface);

  // Called when a Surface's CompositorFrame producer has received a BeginFrame
  // and, thus, is expected to produce damage soon.
  void SurfaceDamageExpected(const SurfaceId& surface_id,
                             const BeginFrameArgs& args);

  void RegisterFrameSinkId(const FrameSinkId& frame_sink_id);

  // Invalidate a frame_sink_id that might still have associated sequences,
  // possibly because a renderer process has crashed.
  void InvalidateFrameSinkId(const FrameSinkId& frame_sink_id);

  const base::flat_map<FrameSinkId, std::string>& valid_frame_sink_labels()
      const {
    return valid_frame_sink_labels_;
  }

  // Set |debug_label| of the |frame_sink_id|. |frame_sink_id| must exist in
  // |valid_frame_sink_labels_| already when UpdateFrameSinkDebugLabel is
  // called.
  void SetFrameSinkDebugLabel(const FrameSinkId& frame_sink_id,
                              const std::string& debug_label);

  // Returns the debug label associated with |frame_sink_id| if any.
  std::string GetFrameSinkDebugLabel(const FrameSinkId& frame_sink_id) const;

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

  // Assigns |owner| as the owner of the temporary reference to
  // |surface_id|. If |owner| is invalidated the temporary reference
  // will be removed. If a surface reference has already been added from the
  // parent to |surface_id| then this will do nothing.
  void AssignTemporaryReference(const SurfaceId& surface_id,
                                const FrameSinkId& owner);

  // Drops the temporary reference for |surface_id|. If a surface reference has
  // already been added from the parent to |surface_id| then this will do
  // nothing.
  void DropTemporaryReference(const SurfaceId& surface_id);

  // Garbage collects all destroyed surfaces that aren't live.
  void GarbageCollectSurfaces();

  // Returns all surfaces referenced by parent |surface_id|. Will return an
  // empty set if |surface_id| is unknown or has no references.
  const base::flat_set<SurfaceId>& GetSurfacesReferencedByParent(
      const SurfaceId& surface_id) const;

  // Returns all surfaces that have a reference to child |surface_id|. Will
  // return an empty set if |surface_id| is unknown or has no references to it.
  const base::flat_set<SurfaceId>& GetSurfacesThatReferenceChild(
      const SurfaceId& surface_id) const;

  // Returns the most recent surface associated with the |fallback_surface_id|'s
  // FrameSinkId that was created prior to the current primary surface and
  // verified by the viz host to be owned by the fallback surface's parent. If
  // the FrameSinkId of the |primary_surface_id| does not match the
  // |fallback_surface_id|'s then this method will always return the fallback
  // surface because we cannot guarantee the latest in flight surface from the
  // fallback frame sink is older than the primary surface.
  Surface* GetLatestInFlightSurface(const SurfaceId& primary_surface_id,
                                    const SurfaceId& fallback_surface_id);

  // Called by SurfaceAggregator notifying us that it will use |surface| in the
  // next display frame. We will notify SurfaceObservers accordingly.
  void SurfaceWillBeDrawn(Surface* surface);

 private:
  friend class test::SurfaceSynchronizationTest;
  friend class test::SurfaceReferencesTest;

  using SurfaceIdSet = std::unordered_set<SurfaceId, SurfaceIdHash>;

  // The reason for removing a temporary reference.
  enum class RemovedReason {
    EMBEDDED,     // The surface was embedded.
    DROPPED,      // The surface won't be embedded so it was dropped.
    SKIPPED,      // A newer surface was embedded and the surface was skipped.
    INVALIDATED,  // The expected embedder was invalidated.
    EXPIRED,      // The surface was never embedded and expired.
    COUNT
  };

  struct SurfaceReferenceInfo {
    SurfaceReferenceInfo();
    ~SurfaceReferenceInfo();

    // Surfaces that have references to this surface.
    base::flat_set<SurfaceId> parents;

    // Surfaces that are referenced from this surface.
    base::flat_set<SurfaceId> children;
  };

  struct TemporaryReferenceData {
    TemporaryReferenceData();
    ~TemporaryReferenceData();

    // The FrameSinkId that is expected to embed this SurfaceId. This will
    // initially be empty and set later by AssignTemporaryReference().
    base::Optional<FrameSinkId> owner;

    // Used to track old surface references, will be marked as true on first
    // timer tick and will be true on second timer tick.
    bool marked_as_old = false;
  };

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

  // Removes temporary reference to |surface_id|. The |reason| for removing will
  // be recorded with UMA. If |reason| is EMBEDDED then older temporary
  // references from the same FrameSinkId will also be removed.
  void RemoveTemporaryReference(const SurfaceId& surface_id,
                                RemovedReason reason);

  // Marks and then expires old temporary references. This function is run
  // periodically by a timer.
  void ExpireOldTemporaryReferences();

  // Removes the surface from the surface map and destroys it.
  void DestroySurfaceInternal(const SurfaceId& surface_id);

#if DCHECK_IS_ON()
  // Recursively prints surface references starting at |surface_id| to |str|.
  void SurfaceReferencesToStringImpl(const SurfaceId& surface_id,
                                     std::string indent,
                                     std::stringstream* str);
#endif

  // Returns true if |surface_id| is in the garbage collector's queue.
  bool IsMarkedForDestruction(const SurfaceId& surface_id);

  base::Optional<uint32_t> activation_deadline_in_frames_;

  // SurfaceDependencyTracker needs to be destroyed after Surfaces are destroyed
  // because they will call back into the dependency tracker.
  SurfaceDependencyTracker dependency_tracker_;

  base::flat_map<SurfaceId, std::unique_ptr<Surface>> surface_map_;
  base::ObserverList<SurfaceObserver> observer_list_;
  base::ThreadChecker thread_checker_;

  base::flat_set<SurfaceId> surfaces_to_destroy_;

  // Set of valid FrameSinkIds and their labels. When a FrameSinkId is removed
  // from this set, any remaining (surface) sequences with that FrameSinkId are
  // considered satisfied.
  base::flat_map<FrameSinkId, std::string> valid_frame_sink_labels_;

  // Root SurfaceId that references display root surfaces. There is no Surface
  // with this id, it's for bookkeeping purposes only.
  const SurfaceId root_surface_id_;

  // Always empty set that is returned when there is no entry in |references_|
  // for a SurfaceId.
  const base::flat_set<SurfaceId> empty_surface_id_set_;

  // Used for setting deadlines for surface synchronization.
  base::TickClock* tick_clock_;

  // Keeps track of surface references for a surface. The graph of references is
  // stored in both directions, so we know the parents and children for each
  // surface.
  std::unordered_map<SurfaceId, SurfaceReferenceInfo, SurfaceIdHash>
      references_;

  // A map of surfaces that have temporary references.
  std::unordered_map<SurfaceId, TemporaryReferenceData, SurfaceIdHash>
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

  // Timer to remove old temporary references that aren't removed after an
  // interval of time. The timer will started/stopped so it only runs if there
  // are temporary references. Also the timer isn't used with Android WebView.
  base::Optional<base::RepeatingTimer> expire_timer_;

  base::WeakPtrFactory<SurfaceManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceManager);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_MANAGER_H_
