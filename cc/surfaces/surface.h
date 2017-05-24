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
#include "cc/output/copy_output_request.h"
#include "cc/surfaces/compositor_frame_sink_support.h"
#include "cc/surfaces/frame_sink_id.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_sequence.h"
#include "cc/surfaces/surfaces_export.h"
#include "ui/gfx/geometry/size.h"

namespace ui {
class LatencyInfo;
}

namespace cc {

class CompositorFrame;
class CopyOutputRequest;

class CC_SURFACES_EXPORT Surface {
 public:
  using WillDrawCallback =
      base::RepeatingCallback<void(const LocalSurfaceId&, const gfx::Rect&)>;

  Surface(
      const SurfaceId& id,
      base::WeakPtr<CompositorFrameSinkSupport> compositor_frame_sink_support);
  ~Surface();

  const SurfaceId& surface_id() const { return surface_id_; }
  const SurfaceId& previous_frame_surface_id() const {
    return previous_frame_surface_id_;
  }

  void SetPreviousFrameSurface(Surface* surface);

  // |draw_callback| is called once to notify the client that the previously
  // submitted CompositorFrame is processed and that another frame can be
  // submitted.
  // |will_draw_callback| is called when |surface| is scheduled for a draw and
  // there is visible damage.
  void QueueFrame(CompositorFrame frame,
                  const base::Closure& draw_callback,
                  const WillDrawCallback& will_draw_callback);
  void RequestCopyOfOutput(std::unique_ptr<CopyOutputRequest> copy_request);

  // Notifies the Surface that a blocking SurfaceId now has an active frame.
  void NotifySurfaceIdAvailable(const SurfaceId& surface_id);

  // Called if a deadline has been hit and this surface is not yet active but
  // it's marked as respecting deadlines.
  void ActivatePendingFrameForDeadline();

  // Adds each CopyOutputRequest in the current frame to copy_requests. The
  // caller takes ownership of them. |copy_requests| is keyed by RenderPass ids.
  void TakeCopyOutputRequests(
      std::multimap<int, std::unique_ptr<CopyOutputRequest>>* copy_requests);

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

  base::WeakPtr<CompositorFrameSinkSupport> compositor_frame_sink_support() {
    return compositor_frame_sink_support_;
  }

  // Add a SurfaceSequence that must be satisfied before the Surface is
  // destroyed.
  void AddDestructionDependency(SurfaceSequence sequence);

  // Satisfy all destruction dependencies that are contained in sequences, and
  // remove them from sequences.
  void SatisfyDestructionDependencies(
      std::unordered_set<SurfaceSequence, SurfaceSequenceHash>* sequences,
      std::unordered_set<FrameSinkId, FrameSinkIdHash>* valid_id_namespaces);
  size_t GetDestructionDependencyCount() const {
    return destruction_dependencies_.size();
  }

  const std::vector<SurfaceId>* active_referenced_surfaces() const {
    return active_frame_data_
               ? &active_frame_data_->frame.metadata.referenced_surfaces
               : nullptr;
  }

  const base::flat_set<SurfaceId>& blocking_surfaces() const {
    return blocking_surfaces_;
  }

  bool HasActiveFrame() const { return active_frame_data_.has_value(); }
  bool HasPendingFrame() const { return pending_frame_data_.has_value(); }

  bool destroyed() const { return destroyed_; }
  void set_destroyed(bool destroyed) { destroyed_ = destroyed; }

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

  // Called to prevent additional CompositorFrames from being accepted into this
  // surface. Once a Surface is closed, it cannot accept CompositorFrames again.
  void Close();

  void ActivatePendingFrame();
  // Called when all of the surface's dependencies have been resolved.
  void ActivateFrame(FrameData frame_data);
  void UpdateBlockingSurfaces(bool has_previous_pending_frame,
                              const CompositorFrame& current_frame);

  void UnrefFrameResourcesAndRunDrawCallback(
      base::Optional<FrameData> frame_data);
  void ClearCopyRequests();

  void TakeLatencyInfoFromPendingFrame(
      std::vector<ui::LatencyInfo>* latency_info);
  static void TakeLatencyInfoFromFrame(
      CompositorFrame* frame,
      std::vector<ui::LatencyInfo>* latency_info);

  const SurfaceId surface_id_;
  SurfaceId previous_frame_surface_id_;
  base::WeakPtr<CompositorFrameSinkSupport> compositor_frame_sink_support_;
  SurfaceManager* const surface_manager_;

  base::Optional<FrameData> pending_frame_data_;
  base::Optional<FrameData> active_frame_data_;
  int frame_index_;
  bool closed_ = false;
  bool destroyed_;
  std::vector<SurfaceSequence> destruction_dependencies_;

  base::flat_set<SurfaceId> blocking_surfaces_;

  DISALLOW_COPY_AND_ASSIGN(Surface);
};

}  // namespace cc

#endif  // CC_SURFACES_SURFACE_H_
