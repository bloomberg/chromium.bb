// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/surface.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>

#include "base/stl_util.h"
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
  for (auto& observer : observers_)
    observer.OnSurfaceDiscarded(this);
  observers_.Clear();

  UnrefFrameResourcesAndRunDrawCallback(std::move(pending_frame_data_));
  UnrefFrameResourcesAndRunDrawCallback(std::move(active_frame_data_));
}

void Surface::SetPreviousFrameSurface(Surface* surface) {
  DCHECK(surface && (HasActiveFrame() || HasPendingFrame()));
  frame_index_ = surface->frame_index() + 1;
  previous_frame_surface_id_ = surface->surface_id();
  CompositorFrame& frame = active_frame_data_ ? active_frame_data_->frame
                                              : pending_frame_data_->frame;
  surface->TakeLatencyInfo(&frame.metadata.latency_info);
  surface->TakeLatencyInfoFromPendingFrame(&frame.metadata.latency_info);
}

void Surface::QueueFrame(CompositorFrame frame, const DrawCallback& callback) {
  TakeLatencyInfoFromPendingFrame(&frame.metadata.latency_info);

  base::Optional<FrameData> previous_pending_frame_data =
      std::move(pending_frame_data_);
  pending_frame_data_.reset();

  UpdateBlockingSurfaces(previous_pending_frame_data.has_value(), frame);

  // Receive and track the resources referenced from the CompositorFrame
  // regardless of whether it's pending or active.
  factory_->ReceiveFromChild(frame.resource_list);

  bool is_pending_frame = !blocking_surfaces_.empty();

  if (is_pending_frame) {
    pending_frame_data_ = FrameData(std::move(frame), callback);
    // Ask the surface manager to inform |this| when its dependencies are
    // resolved.
    factory_->manager()->RequestSurfaceResolution(this);
  } else {
    // If there are no blockers, then immediately activate the frame.
    ActivateFrame(FrameData(std::move(frame), callback));
  }

  // Returns resources for the previous pending frame.
  UnrefFrameResourcesAndRunDrawCallback(std::move(previous_pending_frame_data));
}

void Surface::EvictFrame() {
  QueueFrame(CompositorFrame(), DrawCallback());
  active_frame_data_.reset();
}

void Surface::RequestCopyOfOutput(
    std::unique_ptr<CopyOutputRequest> copy_request) {
  if (!active_frame_data_ ||
      active_frame_data_->frame.render_pass_list.empty()) {
    copy_request->SendEmptyResult();
    return;
  }

  std::vector<std::unique_ptr<CopyOutputRequest>>& copy_requests =
      active_frame_data_->frame.render_pass_list.back()->copy_requests;

  if (copy_request->has_source()) {
    const base::UnguessableToken& source = copy_request->source();
    // Remove existing CopyOutputRequests made on the Surface by the same
    // source.
    base::EraseIf(copy_requests,
                  [&source](const std::unique_ptr<CopyOutputRequest>& x) {
                    return x->has_source() && x->source() == source;
                  });
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
  if (!pending_frame_data_ ||
      !pending_frame_data_->frame.metadata.can_activate_before_dependencies) {
    return;
  }

  // If a frame is being activated because of a deadline, then clear its set
  // of blockers.
  blocking_surfaces_.clear();
  ActivatePendingFrame();
}

Surface::FrameData::FrameData(CompositorFrame&& frame,
                              const DrawCallback& draw_callback)
    : frame(std::move(frame)), draw_callback(draw_callback) {}

Surface::FrameData::FrameData(FrameData&& other) = default;

Surface::FrameData& Surface::FrameData::operator=(FrameData&& other) = default;

Surface::FrameData::~FrameData() = default;

void Surface::ActivatePendingFrame() {
  DCHECK(pending_frame_data_);
  ActivateFrame(std::move(*pending_frame_data_));
  pending_frame_data_.reset();
}

// A frame is activated if all its Surface ID dependences are active or a
// deadline has hit and the frame was forcibly activated by the display
// compositor.
void Surface::ActivateFrame(FrameData frame_data) {
  DCHECK(factory_);

  // Save root pass copy requests.
  std::vector<std::unique_ptr<CopyOutputRequest>> old_copy_requests;
  if (active_frame_data_ &&
      !active_frame_data_->frame.render_pass_list.empty()) {
    std::swap(old_copy_requests,
              active_frame_data_->frame.render_pass_list.back()->copy_requests);
  }

  ClearCopyRequests();

  TakeLatencyInfo(&frame_data.frame.metadata.latency_info);

  base::Optional<FrameData> previous_frame_data = std::move(active_frame_data_);

  active_frame_data_ = std::move(frame_data);

  for (auto& copy_request : old_copy_requests)
    RequestCopyOfOutput(std::move(copy_request));

  // Empty frames shouldn't be drawn and shouldn't contribute damage, so don't
  // increment frame index for them.
  if (!active_frame_data_->frame.render_pass_list.empty())
    ++frame_index_;

  previous_frame_surface_id_ = surface_id();

  UnrefFrameResourcesAndRunDrawCallback(std::move(previous_frame_data));

  for (auto& observer : observers_)
    observer.OnSurfaceActivated(this);
}

void Surface::UpdateBlockingSurfaces(bool has_previous_pending_frame,
                                     const CompositorFrame& current_frame) {
  // If there is no SurfaceDependencyTracker installed then the |current_frame|
  // does not block on anything.
  if (!factory_->manager()->dependency_tracker()) {
    blocking_surfaces_.clear();
    return;
  }

  base::flat_set<SurfaceId> new_blocking_surfaces;

  for (const SurfaceId& surface_id : current_frame.metadata.embedded_surfaces) {
    Surface* surface = factory_->manager()->GetSurfaceForId(surface_id);
    // If a referenced surface does not have a corresponding active frame in the
    // display compositor, then it blocks this frame.
    if (!surface || !surface->HasActiveFrame())
      new_blocking_surfaces.insert(surface_id);
  }

  // If this Surface has a previous pending frame, then we must determine the
  // changes in dependencies so that we can update the SurfaceDependencyTracker
  // map.
  if (has_previous_pending_frame) {
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
  if (!active_frame_data_)
    return;

  for (const auto& render_pass : active_frame_data_->frame.render_pass_list) {
    for (auto& request : render_pass->copy_requests) {
      copy_requests->insert(
          std::make_pair(render_pass->id, std::move(request)));
    }
    render_pass->copy_requests.clear();
  }
}

const CompositorFrame& Surface::GetActiveFrame() const {
  DCHECK(active_frame_data_);
  return active_frame_data_->frame;
}

const CompositorFrame& Surface::GetPendingFrame() {
  DCHECK(pending_frame_data_);
  return pending_frame_data_->frame;
}

void Surface::TakeLatencyInfo(std::vector<ui::LatencyInfo>* latency_info) {
  if (!active_frame_data_)
    return;
  TakeLatencyInfoFromFrame(&active_frame_data_->frame, latency_info);
}

void Surface::RunDrawCallbacks() {
  if (active_frame_data_ && !active_frame_data_->draw_callback.is_null()) {
    DrawCallback callback = active_frame_data_->draw_callback;
    active_frame_data_->draw_callback = DrawCallback();
    callback.Run();
  }
}

void Surface::AddDestructionDependency(SurfaceSequence sequence) {
  destruction_dependencies_.push_back(sequence);
}

void Surface::SatisfyDestructionDependencies(
    std::unordered_set<SurfaceSequence, SurfaceSequenceHash>* sequences,
    std::unordered_set<FrameSinkId, FrameSinkIdHash>* valid_frame_sink_ids) {
  base::EraseIf(destruction_dependencies_,
                [sequences, valid_frame_sink_ids](SurfaceSequence seq) {
                  return (!!sequences->erase(seq) ||
                          !valid_frame_sink_ids->count(seq.frame_sink_id));
                });
}

void Surface::UnrefFrameResourcesAndRunDrawCallback(
    base::Optional<FrameData> frame_data) {
  if (!frame_data || !factory_)
    return;

  ReturnedResourceArray resources;
  TransferableResource::ReturnResources(frame_data->frame.resource_list,
                                        &resources);
  // No point in returning same sync token to sender.
  for (auto& resource : resources)
    resource.sync_token.Clear();
  factory_->UnrefResources(resources);

  if (!frame_data->draw_callback.is_null())
    frame_data->draw_callback.Run();
}

void Surface::ClearCopyRequests() {
  if (active_frame_data_) {
    for (const auto& render_pass : active_frame_data_->frame.render_pass_list) {
      for (const auto& copy_request : render_pass->copy_requests)
        copy_request->SendEmptyResult();
    }
  }
}

void Surface::TakeLatencyInfoFromPendingFrame(
    std::vector<ui::LatencyInfo>* latency_info) {
  if (!pending_frame_data_)
    return;
  TakeLatencyInfoFromFrame(&pending_frame_data_->frame, latency_info);
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
