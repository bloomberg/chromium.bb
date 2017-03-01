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
#include "cc/surfaces/frame_sink_id.h"
#include "cc/surfaces/pending_frame_observer.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_sequence.h"
#include "cc/surfaces/surfaces_export.h"
#include "ui/gfx/geometry/size.h"

namespace ui {
class LatencyInfo;
}

namespace cc {

class BeginFrameSource;
class CompositorFrame;
class CopyOutputRequest;
class SurfaceFactory;

class CC_SURFACES_EXPORT Surface {
 public:
  using DrawCallback = SurfaceFactory::DrawCallback;

  Surface(const SurfaceId& id, base::WeakPtr<SurfaceFactory> factory);
  ~Surface();

  const SurfaceId& surface_id() const { return surface_id_; }
  const SurfaceId& previous_frame_surface_id() const {
    return previous_frame_surface_id_;
  }

  void SetPreviousFrameSurface(Surface* surface);

  void QueueFrame(CompositorFrame frame, const DrawCallback& draw_callback);
  void EvictFrame();
  void RequestCopyOfOutput(std::unique_ptr<CopyOutputRequest> copy_request);

  // Notifies the Surface that a blocking SurfaceId now has an active frame.
  void NotifySurfaceIdAvailable(const SurfaceId& surface_id);

  void AddObserver(PendingFrameObserver* observer);
  void RemoveObserver(PendingFrameObserver* observer);

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
  void RunDrawCallbacks();

  base::WeakPtr<SurfaceFactory> factory() { return factory_; }

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
    return active_frame_ ? &active_frame_->metadata.referenced_surfaces
                         : nullptr;
  }

  const std::vector<SurfaceId>* pending_referenced_surfaces() const {
    return pending_frame_ ? &pending_frame_->metadata.referenced_surfaces
                          : nullptr;
  }

  const SurfaceDependencies& blocking_surfaces_for_testing() const {
    return blocking_surfaces_;
  }

  bool HasActiveFrame() const { return active_frame_.has_value(); }
  bool HasPendingFrame() const { return pending_frame_.has_value(); }

  bool destroyed() const { return destroyed_; }
  void set_destroyed(bool destroyed) { destroyed_ = destroyed; }

 private:
  void ActivatePendingFrame();
  // Called when all of the surface's dependencies have been resolved.
  void ActivateFrame(CompositorFrame frame);
  void UpdateBlockingSurfaces(
      const base::Optional<CompositorFrame>& previous_pending_frame,
      const CompositorFrame& current_frame);

  void UnrefFrameResources(const CompositorFrame& frame_data);
  void ClearCopyRequests();

  void TakeLatencyInfoFromPendingFrame(
      std::vector<ui::LatencyInfo>* latency_info);
  static void TakeLatencyInfoFromFrame(
      CompositorFrame* frame,
      std::vector<ui::LatencyInfo>* latency_info);

  SurfaceId surface_id_;
  SurfaceId previous_frame_surface_id_;
  base::WeakPtr<SurfaceFactory> factory_;
  // TODO(jamesr): Support multiple frames in flight.
  base::Optional<CompositorFrame> pending_frame_;
  base::Optional<CompositorFrame> active_frame_;
  int frame_index_;
  bool destroyed_;
  std::vector<SurfaceSequence> destruction_dependencies_;

  // This surface may have multiple BeginFrameSources if it is
  // on multiple Displays.
  std::set<BeginFrameSource*> begin_frame_sources_;

  SurfaceDependencies blocking_surfaces_;
  base::ObserverList<PendingFrameObserver, true> observers_;

  DrawCallback draw_callback_;

  DISALLOW_COPY_AND_ASSIGN(Surface);
};

using PendingSurfaceSet = base::flat_set<Surface*>;

}  // namespace cc

#endif  // CC_SURFACES_SURFACE_H_
