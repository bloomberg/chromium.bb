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
#include "cc/surfaces/compositor_frame_sink_support.h"
#include "cc/surfaces/local_surface_id_allocator.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/surfaces/surface_resource_holder_client.h"

namespace cc {

// The frame index starts at 2 so that empty frames will be treated as
// completely damaged the first time they're drawn from.
static const int kFrameIndexStart = 2;

Surface::Surface(
    const SurfaceId& id,
    base::WeakPtr<CompositorFrameSinkSupport> compositor_frame_sink_support)
    : surface_id_(id),
      previous_frame_surface_id_(id),
      compositor_frame_sink_support_(std::move(compositor_frame_sink_support)),
      surface_manager_(compositor_frame_sink_support_->surface_manager()),
      frame_index_(kFrameIndexStart),
      destroyed_(false) {}

Surface::~Surface() {
  ClearCopyRequests();
  surface_manager_->SurfaceDiscarded(this);

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

void Surface::Close() {
  closed_ = true;
}

void Surface::QueueFrame(CompositorFrame frame,
                         const base::Closure& callback,
                         const WillDrawCallback& will_draw_callback) {
  if (closed_) {
    if (compositor_frame_sink_support_) {
      ReturnedResourceArray resources;
      TransferableResource::ReturnResources(frame.resource_list, &resources);
      compositor_frame_sink_support_->ReturnResources(resources);
    }
    callback.Run();
    return;
  }

  TakeLatencyInfoFromPendingFrame(&frame.metadata.latency_info);

  base::Optional<FrameData> previous_pending_frame_data =
      std::move(pending_frame_data_);
  pending_frame_data_.reset();

  UpdateBlockingSurfaces(previous_pending_frame_data.has_value(), frame);

  // Receive and track the resources referenced from the CompositorFrame
  // regardless of whether it's pending or active.
  compositor_frame_sink_support_->ReceiveFromChild(frame.resource_list);

  bool is_pending_frame = !blocking_surfaces_.empty();

  if (is_pending_frame) {
    // Reject CompositorFrames submitted to surfaces referenced from this
    // CompositorFrame as fallbacks. This saves some CPU cycles to allow
    // children to catch up to the parent.
    base::flat_set<FrameSinkId> frame_sink_ids_for_dependencies;
    for (const SurfaceId& surface_id : frame.metadata.activation_dependencies)
      frame_sink_ids_for_dependencies.insert(surface_id.frame_sink_id());
    for (const SurfaceId& surface_id : frame.metadata.referenced_surfaces) {
      // A surface ID in |referenced_surfaces| that has a corresponding surface
      // ID in |activation_dependencies| with the same frame sink ID is said to
      // be a fallback surface that can be used in place of the primary surface
      // if the deadline passes before the dependency becomes available.
      bool is_fallback_surface =
          frame_sink_ids_for_dependencies.count(surface_id.frame_sink_id()) > 0;
      if (is_fallback_surface) {
        Surface* surface = surface_manager_->GetSurfaceForId(surface_id);
        DCHECK(surface);
        surface->Close();
      }
    }
    pending_frame_data_ =
        FrameData(std::move(frame), callback, will_draw_callback);
    // Ask the surface manager to inform |this| when its dependencies are
    // resolved.
    surface_manager_->RequestSurfaceResolution(this);
  } else {
    // If there are no blockers, then immediately activate the frame.
    ActivateFrame(FrameData(std::move(frame), callback, will_draw_callback));
  }

  // Returns resources for the previous pending frame.
  UnrefFrameResourcesAndRunDrawCallback(std::move(previous_pending_frame_data));
}

void Surface::RequestCopyOfOutput(
    std::unique_ptr<CopyOutputRequest> copy_request) {
  if (!active_frame_data_) {
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
                              const base::Closure& draw_callback,
                              const WillDrawCallback& will_draw_callback)
    : frame(std::move(frame)),
      draw_callback(draw_callback),
      will_draw_callback(will_draw_callback) {}

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
  DCHECK(compositor_frame_sink_support_);

  // Save root pass copy requests.
  std::vector<std::unique_ptr<CopyOutputRequest>> old_copy_requests;
  if (active_frame_data_) {
    std::swap(old_copy_requests,
              active_frame_data_->frame.render_pass_list.back()->copy_requests);
  }

  ClearCopyRequests();

  TakeLatencyInfo(&frame_data.frame.metadata.latency_info);

  base::Optional<FrameData> previous_frame_data = std::move(active_frame_data_);

  active_frame_data_ = std::move(frame_data);

  for (auto& copy_request : old_copy_requests)
    RequestCopyOfOutput(std::move(copy_request));

  ++frame_index_;

  previous_frame_surface_id_ = surface_id();

  UnrefFrameResourcesAndRunDrawCallback(std::move(previous_frame_data));

  compositor_frame_sink_support_->OnSurfaceActivated(this);
}

void Surface::UpdateBlockingSurfaces(bool has_previous_pending_frame,
                                     const CompositorFrame& current_frame) {
  // If there is no SurfaceDependencyTracker installed then the |current_frame|
  // does not block on anything.
  if (!surface_manager_->dependency_tracker()) {
    blocking_surfaces_.clear();
    return;
  }

  base::flat_set<SurfaceId> new_blocking_surfaces;

  for (const SurfaceId& surface_id :
       current_frame.metadata.activation_dependencies) {
    Surface* surface = surface_manager_->GetSurfaceForId(surface_id);
    // If a referenced surface does not have a corresponding active frame in the
    // display compositor, then it blocks this frame.
    if (!surface || !surface->HasActiveFrame())
      new_blocking_surfaces.insert(surface_id);
  }

  // If this Surface has a previous pending frame, then we must determine the
  // changes in dependencies so that we can update the SurfaceDependencyTracker
  // map.
  if (has_previous_pending_frame) {
    base::flat_set<SurfaceId> removed_dependencies;
    for (const SurfaceId& surface_id : blocking_surfaces_) {
      if (!new_blocking_surfaces.count(surface_id))
        removed_dependencies.insert(surface_id);
    }

    base::flat_set<SurfaceId> added_dependencies;
    for (const SurfaceId& surface_id : new_blocking_surfaces) {
      if (!blocking_surfaces_.count(surface_id))
        added_dependencies.insert(surface_id);
    }

    // If there is a change in the dependency set, then inform observers.
    if (!added_dependencies.empty() || !removed_dependencies.empty()) {
      surface_manager_->SurfaceDependenciesChanged(this, added_dependencies,
                                                   removed_dependencies);
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

void Surface::RunDrawCallback() {
  if (active_frame_data_ && !active_frame_data_->draw_callback.is_null()) {
    base::Closure callback = active_frame_data_->draw_callback;
    active_frame_data_->draw_callback = base::Closure();
    callback.Run();
  }
}

void Surface::RunWillDrawCallback(const gfx::Rect& damage_rect) {
  if (!active_frame_data_ || active_frame_data_->will_draw_callback.is_null())
    return;

  active_frame_data_->will_draw_callback.Run(surface_id_.local_surface_id(),
                                             damage_rect);
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
  if (!frame_data || !compositor_frame_sink_support_)
    return;

  ReturnedResourceArray resources;
  TransferableResource::ReturnResources(frame_data->frame.resource_list,
                                        &resources);
  // No point in returning same sync token to sender.
  for (auto& resource : resources)
    resource.sync_token.Clear();
  compositor_frame_sink_support_->UnrefResources(resources);

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
