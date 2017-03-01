// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/surface.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>

#include "cc/base/container_util.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/copy_output_request.h"
#include "cc/surfaces/local_surface_id_allocator.h"
#include "cc/surfaces/pending_frame_observer.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_manager.h"

namespace cc {

// The frame index starts at 2 so that empty frames will be treated as
// completely damaged the first time they're drawn from.
static const int kFrameIndexStart = 2;

Surface::Surface(const SurfaceId& id, base::WeakPtr<SurfaceFactory> factory)
    : surface_id_(id),
      previous_frame_surface_id_(id),
      factory_(factory),
      frame_index_(kFrameIndexStart),
      destroyed_(false) {}

Surface::~Surface() {
  ClearCopyRequests();
  if (factory_) {
    if (pending_frame_)
      UnrefFrameResources(*pending_frame_);
    if (active_frame_)
      UnrefFrameResources(*active_frame_);
  }
  if (!draw_callback_.is_null())
    draw_callback_.Run();

  for (auto& observer : observers_)
    observer.OnSurfaceDiscarded(this);
  observers_.Clear();
}

void Surface::SetPreviousFrameSurface(Surface* surface) {
  DCHECK(surface);
  frame_index_ = surface->frame_index() + 1;
  previous_frame_surface_id_ = surface->surface_id();
  CompositorFrame& frame = active_frame_ ? *active_frame_ : *pending_frame_;
  surface->TakeLatencyInfo(&frame.metadata.latency_info);
  surface->TakeLatencyInfoFromPendingFrame(&frame.metadata.latency_info);
}

void Surface::QueueFrame(CompositorFrame frame, const DrawCallback& callback) {
  TakeLatencyInfoFromPendingFrame(&frame.metadata.latency_info);

  base::Optional<CompositorFrame> previous_pending_frame =
      std::move(pending_frame_);
  pending_frame_.reset();

  UpdateBlockingSurfaces(previous_pending_frame, frame);

  // Receive and track the resources referenced from the CompositorFrame
  // regardless of whether it's pending or active.
  factory_->ReceiveFromChild(frame.resource_list);

  if (!blocking_surfaces_.empty()) {
    pending_frame_ = std::move(frame);
    // Ask the surface manager to inform |this| when its dependencies are
    // resolved.
    factory_->manager()->RequestSurfaceResolution(this);

    // We do not have to notify observers that referenced surfaces have changed
    // in the else case because ActivateFrame will notify observers.
    for (auto& observer : observers_) {
      observer.OnReferencedSurfacesChanged(this, active_referenced_surfaces(),
                                           pending_referenced_surfaces());
    }
  } else {
    // If there are no blockers, then immediately activate the frame.
    ActivateFrame(std::move(frame));
  }

  // Returns resources for the previous pending frame.
  if (previous_pending_frame)
    UnrefFrameResources(*previous_pending_frame);

  if (!draw_callback_.is_null())
    draw_callback_.Run();
  draw_callback_ = callback;
}

void Surface::EvictFrame() {
  QueueFrame(CompositorFrame(), DrawCallback());
  active_frame_.reset();
}

void Surface::RequestCopyOfOutput(
    std::unique_ptr<CopyOutputRequest> copy_request) {
  if (!active_frame_ || active_frame_->render_pass_list.empty()) {
    copy_request->SendEmptyResult();
    return;
  }

  std::vector<std::unique_ptr<CopyOutputRequest>>& copy_requests =
      active_frame_->render_pass_list.back()->copy_requests;

  if (copy_request->has_source()) {
    const base::UnguessableToken& source = copy_request->source();
    // Remove existing CopyOutputRequests made on the Surface by the same
    // source.
    auto to_remove =
        std::remove_if(copy_requests.begin(), copy_requests.end(),
                       [&source](const std::unique_ptr<CopyOutputRequest>& x) {
                         return x->has_source() && x->source() == source;
                       });
    copy_requests.erase(to_remove, copy_requests.end());
  }
  copy_requests.push_back(std::move(copy_request));
}

void Surface::NotifySurfaceIdAvailable(const SurfaceId& surface_id) {
  auto it = blocking_surfaces_.find(surface_id);
  // This surface may no longer have blockers if the deadline has passed.
  if (it == blocking_surfaces_.end())
    return;

  blocking_surfaces_.erase(it);

  if (!blocking_surfaces_.empty())
    return;

  // All blockers have been cleared. The surface can be activated now.
  ActivatePendingFrame();
}

void Surface::AddObserver(PendingFrameObserver* observer) {
  observers_.AddObserver(observer);
}

void Surface::RemoveObserver(PendingFrameObserver* observer) {
  observers_.RemoveObserver(observer);
}

void Surface::ActivatePendingFrameForDeadline() {
  if (!pending_frame_ ||
      !pending_frame_->metadata.can_activate_before_dependencies) {
    return;
  }

  // If a frame is being activated because of a deadline, then clear its set
  // of blockers.
  blocking_surfaces_.clear();
  ActivatePendingFrame();
}

void Surface::ActivatePendingFrame() {
  DCHECK(pending_frame_);
  ActivateFrame(std::move(pending_frame_.value()));
  pending_frame_.reset();
}

// A frame is activated if all its Surface ID dependences are active or a
// deadline has hit and the frame was forcibly activated by the display
// compositor.
void Surface::ActivateFrame(CompositorFrame frame) {
  DCHECK(factory_);

  // Save root pass copy requests.
  std::vector<std::unique_ptr<CopyOutputRequest>> old_copy_requests;
  if (active_frame_ && !active_frame_->render_pass_list.empty()) {
    std::swap(old_copy_requests,
              active_frame_->render_pass_list.back()->copy_requests);
  }

  ClearCopyRequests();

  TakeLatencyInfo(&frame.metadata.latency_info);

  base::Optional<CompositorFrame> previous_frame = std::move(active_frame_);
  active_frame_ = std::move(frame);

  for (auto& copy_request : old_copy_requests)
    RequestCopyOfOutput(std::move(copy_request));

  // Empty frames shouldn't be drawn and shouldn't contribute damage, so don't
  // increment frame index for them.
  if (!active_frame_->render_pass_list.empty())
    ++frame_index_;

  previous_frame_surface_id_ = surface_id();

  if (previous_frame)
    UnrefFrameResources(*previous_frame);

  for (auto& observer : observers_)
    observer.OnSurfaceActivated(this);

  for (auto& observer : observers_) {
    observer.OnReferencedSurfacesChanged(this, active_referenced_surfaces(),
                                         pending_referenced_surfaces());
  }
}

void Surface::UpdateBlockingSurfaces(
    const base::Optional<CompositorFrame>& previous_pending_frame,
    const CompositorFrame& current_frame) {
  // If there is no SurfaceDependencyTracker installed then the |current_frame|
  // does not block on anything.
  if (!factory_->manager()->dependency_tracker()) {
    blocking_surfaces_.clear();
    return;
  }

  base::flat_set<SurfaceId> new_blocking_surfaces;

  for (const SurfaceId& surface_id :
       current_frame.metadata.referenced_surfaces) {
    Surface* surface = factory_->manager()->GetSurfaceForId(surface_id);
    // If a referenced surface does not have a corresponding active frame in the
    // display compositor, then it blocks this frame.
    if (!surface || !surface->HasActiveFrame())
      new_blocking_surfaces.insert(surface_id);
  }

  // If this Surface has a previous pending frame, then we must determine the
  // changes in dependencies so that we can update the SurfaceDependencyTracker
  // map.
  if (previous_pending_frame.has_value()) {
    SurfaceDependencies removed_dependencies;
    for (const SurfaceId& surface_id : blocking_surfaces_) {
      if (!new_blocking_surfaces.count(surface_id))
        removed_dependencies.insert(surface_id);
    }

    SurfaceDependencies added_dependencies;
    for (const SurfaceId& surface_id : new_blocking_surfaces) {
      if (!blocking_surfaces_.count(surface_id))
        added_dependencies.insert(surface_id);
    }

    // If there is a change in the dependency set, then inform observers.
    if (!added_dependencies.empty() || !removed_dependencies.empty()) {
      for (auto& observer : observers_) {
        observer.OnSurfaceDependenciesChanged(this, added_dependencies,
                                              removed_dependencies);
      }
    }
  }

  blocking_surfaces_ = std::move(new_blocking_surfaces);
}

void Surface::TakeCopyOutputRequests(
    std::multimap<int, std::unique_ptr<CopyOutputRequest>>* copy_requests) {
  DCHECK(copy_requests->empty());
  if (!active_frame_)
    return;

  for (const auto& render_pass : active_frame_->render_pass_list) {
    for (auto& request : render_pass->copy_requests) {
      copy_requests->insert(
          std::make_pair(render_pass->id, std::move(request)));
    }
    render_pass->copy_requests.clear();
  }
}

const CompositorFrame& Surface::GetActiveFrame() const {
  DCHECK(active_frame_);
  return active_frame_.value();
}

const CompositorFrame& Surface::GetPendingFrame() {
  DCHECK(pending_frame_);
  return pending_frame_.value();
}

void Surface::TakeLatencyInfo(std::vector<ui::LatencyInfo>* latency_info) {
  if (!active_frame_)
    return;
  TakeLatencyInfoFromFrame(&active_frame_.value(), latency_info);
}

void Surface::RunDrawCallbacks() {
  if (!draw_callback_.is_null()) {
    DrawCallback callback = draw_callback_;
    draw_callback_ = DrawCallback();
    callback.Run();
  }
}

void Surface::AddDestructionDependency(SurfaceSequence sequence) {
  destruction_dependencies_.push_back(sequence);
}

void Surface::SatisfyDestructionDependencies(
    std::unordered_set<SurfaceSequence, SurfaceSequenceHash>* sequences,
    std::unordered_set<FrameSinkId, FrameSinkIdHash>* valid_frame_sink_ids) {
  destruction_dependencies_.erase(
      std::remove_if(destruction_dependencies_.begin(),
                     destruction_dependencies_.end(),
                     [sequences, valid_frame_sink_ids](SurfaceSequence seq) {
                       return (!!sequences->erase(seq) ||
                               !valid_frame_sink_ids->count(seq.frame_sink_id));
                     }),
      destruction_dependencies_.end());
}

void Surface::UnrefFrameResources(const CompositorFrame& frame) {
  ReturnedResourceArray resources;
  TransferableResource::ReturnResources(frame.resource_list, &resources);
  // No point in returning same sync token to sender.
  for (auto& resource : resources)
    resource.sync_token.Clear();
  factory_->UnrefResources(resources);
}

void Surface::ClearCopyRequests() {
  if (active_frame_) {
    for (const auto& render_pass : active_frame_->render_pass_list) {
      for (const auto& copy_request : render_pass->copy_requests)
        copy_request->SendEmptyResult();
    }
  }
}

void Surface::TakeLatencyInfoFromPendingFrame(
    std::vector<ui::LatencyInfo>* latency_info) {
  if (!pending_frame_)
    return;
  TakeLatencyInfoFromFrame(&pending_frame_.value(), latency_info);
}

// static
void Surface::TakeLatencyInfoFromFrame(
    CompositorFrame* frame,
    std::vector<ui::LatencyInfo>* latency_info) {
  if (latency_info->empty()) {
    frame->metadata.latency_info.swap(*latency_info);
    return;
  }
  std::copy(frame->metadata.latency_info.begin(),
            frame->metadata.latency_info.end(),
            std::back_inserter(*latency_info));
  frame->metadata.latency_info.clear();
}

}  // namespace cc
