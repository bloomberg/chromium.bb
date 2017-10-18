// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_H_
#define COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <unordered_set>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/common/surfaces/surface_sequence.h"
#include "components/viz/service/surfaces/surface_dependency_deadline.h"
#include "components/viz/service/viz_service_export.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
class CopyOutputRequest;
}

namespace ui {
class LatencyInfo;
}

namespace viz {

class SurfaceClient;
class SurfaceManager;

// A Surface is a representation of a sequence of CompositorFrames with a
// common set of properties uniquely identified by a SurfaceId. In particular,
// all CompositorFrames submitted to a single Surface share properties described
// in SurfaceInfo: device scale factor and size. A Surface can hold up to two
// CompositorFrames at a given time:
//
//   Active frame:  An active frame is a candidate for display. A
//                  CompositorFrame is active if it has been explicitly marked
//                  as active after a deadline has passed or all its
//                  dependencies are active.
//
//   Pending frame: A pending CompositorFrame cannot be displayed on screen. A
//                  CompositorFrame is pending if it has unresolved
//                  dependencies: surface Ids to which there are no active
//                  CompositorFrames.
//
// This two stage mechanism for managing CompositorFrames from a client exists
// to enable best-effort synchronization across clients. A surface subtree will
// remain pending until all dependencies are resolved: all clients have
// submitted CompositorFrames corresponding to a new property of the subtree
// (e.g. a new size).
//
// Clients are assumed to be untrusted and so a client may not submit a
// CompositorFrame to satisfy the dependency of the parent. Thus, by default, a
// surface has an activation deadline associated with its dependencies. If the
// deadline passes, then the CompositorFrame will activate despite missing
// dependencies. The activated CompositorFrame can specify fallback behavior in
// the event of missing dependencies at display time.
class VIZ_SERVICE_EXPORT Surface final : public SurfaceDeadlineClient {
 public:
  using AggregatedDamageCallback =
      base::RepeatingCallback<void(const LocalSurfaceId&, const gfx::Rect&)>;

  Surface(const SurfaceInfo& surface_info,
          SurfaceManager* surface_manager,
          base::WeakPtr<SurfaceClient> surface_client,
          BeginFrameSource* begin_frame_source,
          bool needs_sync_tokens);
  ~Surface();

  // Clears the |seen_first_frame_activation_| bit causing a
  // FirstSurfaceActivation to be triggered on the next CompositorFrame
  // activation.
  void ResetSeenFirstFrameActivation();

  const SurfaceId& surface_id() const { return surface_info_.id(); }
  const SurfaceId& previous_frame_surface_id() const {
    return previous_frame_surface_id_;
  }

  base::WeakPtr<SurfaceClient> client() { return surface_client_; }

  bool has_deadline() const { return deadline_.has_deadline(); }
  const SurfaceDependencyDeadline& deadline() const { return deadline_; }

  bool InheritActivationDeadlineFrom(
      const SurfaceDependencyDeadline& deadline) {
    return deadline_.InheritFrom(deadline);
  }

  // Sets a deadline a number of frames ahead to active the currently pending
  // CompositorFrame held by this surface.
  void SetActivationDeadline(uint32_t number_of_frames_to_deadline) {
    deadline_.Set(number_of_frames_to_deadline);
  }

  void SetPreviousFrameSurface(Surface* surface);

  // Increments the reference count on resources specified by |resources|.
  void RefResources(const std::vector<TransferableResource>& resources);

  // Decrements the reference count on resources specified by |resources|.
  void UnrefResources(const std::vector<ReturnedResource>& resources);

  bool needs_sync_tokens() const { return needs_sync_tokens_; }

  // Returns false if |frame| is invalid.
  // |draw_callback| is called once to notify the client that the previously
  // submitted CompositorFrame is processed and that another frame can be
  // there is visible damage.
  // |aggregated_damage_callback| is called when |surface| or one of its
  // descendents is determined to be damaged at aggregation time.
  bool QueueFrame(CompositorFrame frame,
                  uint64_t frame_index,
                  base::OnceClosure draw_callback,
                  const AggregatedDamageCallback& aggregated_damage_callback);
  void RequestCopyOfOutput(std::unique_ptr<CopyOutputRequest> copy_request);

  // Notifies the Surface that a blocking SurfaceId now has an active
  // frame.
  void NotifySurfaceIdAvailable(const SurfaceId& surface_id);

  // Called if a deadline has been hit and this surface is not yet active but
  // it's marked as respecting deadlines.
  void ActivatePendingFrameForDeadline();

  using CopyRequestsMap =
      std::multimap<RenderPassId, std::unique_ptr<CopyOutputRequest>>;

  // Adds each CopyOutputRequest in the current frame to copy_requests. The
  // caller takes ownership of them. |copy_requests| is keyed by RenderPass
  // ids.
  void TakeCopyOutputRequests(CopyRequestsMap* copy_requests);

  // Returns the most recent frame that is eligible to be rendered.
  // You must check whether HasActiveFrame() returns true before calling this
  // method.
  const CompositorFrame& GetActiveFrame() const;

  // Returns the currently pending frame. You must check where HasPendingFrame()
  // returns true before calling this method.
  const CompositorFrame& GetPendingFrame();

  // Returns a number that increments by 1 every time a new frame is enqueued.
  uint64_t GetActiveFrameIndex() const {
    return active_frame_data_->frame_index;
  }

  void TakeLatencyInfo(std::vector<ui::LatencyInfo>* latency_info);
  void RunDrawCallback();
  void NotifyAggregatedDamage(const gfx::Rect& damage_rect);

  // Add a SurfaceSequence that must be satisfied before the Surface is
  // destroyed.
  void AddDestructionDependency(SurfaceSequence sequence);

  // Satisfy all destruction dependencies that are contained in sequences, and
  // remove them from sequences.
  void SatisfyDestructionDependencies(
      base::flat_set<SurfaceSequence>* sequences,
      base::flat_map<FrameSinkId, std::string>* valid_id_namespaces);
  size_t GetDestructionDependencyCount() const {
    return destruction_dependencies_.size();
  }

  const std::vector<SurfaceId>* active_referenced_surfaces() const {
    return active_frame_data_
               ? &active_frame_data_->frame.metadata.referenced_surfaces
               : nullptr;
  }

  // Returns the set of dependencies blocking this surface's pending frame
  // that themselves have not yet activated.
  const base::flat_set<SurfaceId>& activation_dependencies() const {
    return activation_dependencies_;
  }

  // Returns the set of activation dependencies that have been ignored because
  // the last CompositorFrame was activated due to a deadline. Late
  // dependencies activate immediately when they arrive.
  const base::flat_set<SurfaceId>& late_activation_dependencies() const {
    return late_activation_dependencies_;
  }

  bool HasActiveFrame() const { return active_frame_data_.has_value(); }
  bool HasPendingFrame() const { return pending_frame_data_.has_value(); }
  bool HasUndrawnActiveFrame() const {
    return HasActiveFrame() && active_frame_data_->draw_callback;
  }

  // SurfaceDeadlineClient implementation:
  void OnDeadline() override;

 private:
  struct FrameData {
    FrameData(CompositorFrame&& frame,
              uint64_t frame_index,
              base::OnceClosure draw_callback,
              const AggregatedDamageCallback& aggregated_damage_callback);
    FrameData(FrameData&& other);
    ~FrameData();
    FrameData& operator=(FrameData&& other);
    CompositorFrame frame;
    uint64_t frame_index;
    base::OnceClosure draw_callback;
    AggregatedDamageCallback aggregated_damage_callback;
  };

  // Rejects CompositorFrames submitted to surfaces referenced from this
  // CompositorFrame as fallbacks. This saves some CPU cycles to allow
  // children to catch up to the parent.
  void RejectCompositorFramesToFallbackSurfaces();

  // Called to prevent additional CompositorFrames from being accepted into this
  // surface. Once a Surface is closed, it cannot accept CompositorFrames again.
  void Close();

  void ActivatePendingFrame();
  // Called when all of the surface's dependencies have been resolved.
  void ActivateFrame(FrameData frame_data);
  void UpdateActivationDependencies(const CompositorFrame& current_frame);
  void ComputeChangeInDependencies(
      const base::flat_set<SurfaceId>& existing_dependencies,
      const base::flat_set<SurfaceId>& new_dependencies,
      base::flat_set<SurfaceId>* added_dependencies,
      base::flat_set<SurfaceId>* removed_dependencies);

  void UnrefFrameResourcesAndRunDrawCallback(
      base::Optional<FrameData> frame_data);
  void ClearCopyRequests();

  void TakeLatencyInfoFromPendingFrame(
      std::vector<ui::LatencyInfo>* latency_info);
  static void TakeLatencyInfoFromFrame(
      CompositorFrame* frame,
      std::vector<ui::LatencyInfo>* latency_info);

  const SurfaceInfo surface_info_;
  SurfaceId previous_frame_surface_id_;
  SurfaceManager* const surface_manager_;
  base::WeakPtr<SurfaceClient> surface_client_;
  SurfaceDependencyDeadline deadline_;

  base::Optional<FrameData> pending_frame_data_;
  base::Optional<FrameData> active_frame_data_;
  bool closed_ = false;
  bool seen_first_frame_activation_ = false;
  const bool needs_sync_tokens_;
  std::vector<SurfaceSequence> destruction_dependencies_;

  base::flat_set<SurfaceId> activation_dependencies_;
  base::flat_set<SurfaceId> late_activation_dependencies_;

  DISALLOW_COPY_AND_ASSIGN(Surface);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_H_
