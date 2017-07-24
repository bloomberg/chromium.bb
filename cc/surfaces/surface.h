// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_SURFACE_H_
#define CC_SURFACES_SURFACE_H_

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
#include "cc/output/compositor_frame.h"
#include "cc/output/copy_output_request.h"
#include "cc/surfaces/surface_dependency_deadline.h"
#include "cc/surfaces/surfaces_export.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/common/surfaces/surface_sequence.h"
#include "ui/gfx/geometry/size.h"

namespace ui {
class LatencyInfo;
}

namespace cc {

class SurfaceClient;
class CopyOutputRequest;
class SurfaceManager;

class CC_SURFACES_EXPORT Surface : public SurfaceDeadlineObserver {
 public:
  using WillDrawCallback =
      base::RepeatingCallback<void(const viz::LocalSurfaceId&,
                                   const gfx::Rect&)>;

  Surface(const viz::SurfaceInfo& surface_info,
          SurfaceManager* surface_manager,
          base::WeakPtr<SurfaceClient> surface_client,
          viz::BeginFrameSource* begin_frame_source,
          bool needs_sync_tokens);
  ~Surface();

  const viz::SurfaceId& surface_id() const { return surface_info_.id(); }
  const viz::SurfaceId& previous_frame_surface_id() const {
    return previous_frame_surface_id_;
  }

  base::WeakPtr<SurfaceClient> client() { return surface_client_; }

  bool has_deadline() const { return deadline_.has_deadline(); }
  const SurfaceDependencyDeadline& deadline() const { return deadline_; }

  bool InheritActivationDeadlineFrom(
      const SurfaceDependencyDeadline& deadline) {
    return deadline_.InheritFrom(deadline);
  }

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
  // |will_draw_callback| is called when |surface| is scheduled for a draw and
  // there is visible damage.
  bool QueueFrame(CompositorFrame frame,
                  const base::Closure& draw_callback,
                  const WillDrawCallback& will_draw_callback);
  void RequestCopyOfOutput(std::unique_ptr<CopyOutputRequest> copy_request);

  // Notifies the Surface that a blocking viz::SurfaceId now has an active
  // frame.
  void NotifySurfaceIdAvailable(const viz::SurfaceId& surface_id);

  // Called if a deadline has been hit and this surface is not yet active but
  // it's marked as respecting deadlines.
  void ActivatePendingFrameForDeadline();

  using CopyRequestsMap =
      std::multimap<RenderPassId, std::unique_ptr<CopyOutputRequest>>;

  // Adds each CopyOutputRequest in the current frame to copy_requests. The
  // caller takes ownership of them. |copy_requests| is keyed by RenderPass ids.
  void TakeCopyOutputRequests(CopyRequestsMap* copy_requests);

  // Returns the most recent frame that is eligible to be rendered.
  // You must check whether HasActiveFrame() returns true before calling this
  // method.
  const CompositorFrame& GetActiveFrame() const;

  // Returns the currently pending frame. You must check where HasPendingFrame()
  // returns true before calling this method.
  const CompositorFrame& GetPendingFrame();

  // Returns a number that increments by 1 every time a new frame is enqueued.
  int frame_index() const { return frame_index_; }

  void TakeLatencyInfo(std::vector<ui::LatencyInfo>* latency_info);
  void RunDrawCallback();
  void RunWillDrawCallback(const gfx::Rect& damage_rect);

  // Add a viz::SurfaceSequence that must be satisfied before the Surface is
  // destroyed.
  void AddDestructionDependency(viz::SurfaceSequence sequence);

  // Satisfy all destruction dependencies that are contained in sequences, and
  // remove them from sequences.
  void SatisfyDestructionDependencies(
      base::flat_set<viz::SurfaceSequence>* sequences,
      base::flat_set<viz::FrameSinkId>* valid_id_namespaces);
  size_t GetDestructionDependencyCount() const {
    return destruction_dependencies_.size();
  }

  const std::vector<viz::SurfaceId>* active_referenced_surfaces() const {
    return active_frame_data_
               ? &active_frame_data_->frame.metadata.referenced_surfaces
               : nullptr;
  }

  // Returns the set of dependencies blocking this surface's pending frame
  // that themselves have not yet activated.
  const base::flat_set<viz::SurfaceId>& activation_dependencies() const {
    return activation_dependencies_;
  }

  // Returns the set of activation dependencies that have been ignored because
  // the last CompositorFrame was activated due to a deadline. Late dependencies
  // activate immediately when they arrive.
  const base::flat_set<viz::SurfaceId>& late_activation_dependencies() const {
    return late_activation_dependencies_;
  }

  bool HasActiveFrame() const { return active_frame_data_.has_value(); }
  bool HasPendingFrame() const { return pending_frame_data_.has_value(); }
  bool HasUndrawnActiveFrame() const {
    return HasActiveFrame() && active_frame_data_->draw_callback;
  }

  // SurfaceDeadlineObserver implementation:
  void OnDeadline() override;

 private:
  struct FrameData {
    FrameData(CompositorFrame&& frame,
              const base::Closure& draw_callback,
              const WillDrawCallback& will_draw_callback);
    FrameData(FrameData&& other);
    ~FrameData();
    FrameData& operator=(FrameData&& other);
    CompositorFrame frame;
    base::Closure draw_callback;
    WillDrawCallback will_draw_callback;
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
      const base::flat_set<viz::SurfaceId>& existing_dependencies,
      const base::flat_set<viz::SurfaceId>& new_dependencies,
      base::flat_set<viz::SurfaceId>* added_dependencies,
      base::flat_set<viz::SurfaceId>* removed_dependencies);

  void UnrefFrameResourcesAndRunDrawCallback(
      base::Optional<FrameData> frame_data);
  void ClearCopyRequests();

  void TakeLatencyInfoFromPendingFrame(
      std::vector<ui::LatencyInfo>* latency_info);
  static void TakeLatencyInfoFromFrame(
      CompositorFrame* frame,
      std::vector<ui::LatencyInfo>* latency_info);

  viz::SurfaceInfo surface_info_;
  viz::SurfaceId previous_frame_surface_id_;
  SurfaceManager* const surface_manager_;
  base::WeakPtr<SurfaceClient> surface_client_;
  SurfaceDependencyDeadline deadline_;

  base::Optional<FrameData> pending_frame_data_;
  base::Optional<FrameData> active_frame_data_;
  int frame_index_;
  bool closed_ = false;
  const bool needs_sync_tokens_;
  std::vector<viz::SurfaceSequence> destruction_dependencies_;

  base::flat_set<viz::SurfaceId> activation_dependencies_;
  base::flat_set<viz::SurfaceId> late_activation_dependencies_;

  DISALLOW_COPY_AND_ASSIGN(Surface);
};

}  // namespace cc

#endif  // CC_SURFACES_SURFACE_H_
