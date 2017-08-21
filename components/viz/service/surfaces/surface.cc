// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/surfaces/surface.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>

#include "base/stl_util.h"
#include "components/viz/common/quads/copy_output_request.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/common/surfaces/local_surface_id_allocator.h"
#include "components/viz/service/surfaces/surface_client.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

Surface::Surface(const SurfaceInfo& surface_info,
                 SurfaceManager* surface_manager,
                 base::WeakPtr<SurfaceClient> surface_client,
                 BeginFrameSource* begin_frame_source,
                 bool needs_sync_tokens)
    : surface_info_(surface_info),
      surface_manager_(surface_manager),
      surface_client_(std::move(surface_client)),
      deadline_(begin_frame_source),
      needs_sync_tokens_(needs_sync_tokens) {
  deadline_.AddObserver(this);
}

Surface::~Surface() {
  ClearCopyRequests();
  surface_manager_->SurfaceDiscarded(this);

  UnrefFrameResourcesAndRunDrawCallback(std::move(pending_frame_data_));
  UnrefFrameResourcesAndRunDrawCallback(std::move(active_frame_data_));

  deadline_.Cancel();
  deadline_.RemoveObserver(this);
}

void Surface::SetPreviousFrameSurface(Surface* surface) {
  DCHECK(surface && (HasActiveFrame() || HasPendingFrame()));
  previous_frame_surface_id_ = surface->surface_id();
  cc::CompositorFrame& frame = active_frame_data_ ? active_frame_data_->frame
                                                  : pending_frame_data_->frame;
  surface->TakeLatencyInfo(&frame.metadata.latency_info);
  surface->TakeLatencyInfoFromPendingFrame(&frame.metadata.latency_info);
}

void Surface::RefResources(const std::vector<TransferableResource>& resources) {
  if (surface_client_)
    surface_client_->RefResources(resources);
}

void Surface::UnrefResources(const std::vector<ReturnedResource>& resources) {
  if (surface_client_)
    surface_client_->UnrefResources(resources);
}

void Surface::RejectCompositorFramesToFallbackSurfaces() {
  std::vector<FrameSinkId> frame_sink_ids_for_dependencies;
  for (const SurfaceId& surface_id :
       GetPendingFrame().metadata.activation_dependencies) {
    frame_sink_ids_for_dependencies.push_back(surface_id.frame_sink_id());
  }

  for (const SurfaceId& surface_id :
       GetPendingFrame().metadata.referenced_surfaces) {
    // A surface ID in |referenced_surfaces| that has a corresponding surface
    // ID in |activation_dependencies| with the same frame sink ID is said to
    // be a fallback surface that can be used in place of the primary surface
    // if the deadline passes before the dependency becomes available.
    auto it = std::find(frame_sink_ids_for_dependencies.begin(),
                        frame_sink_ids_for_dependencies.end(),
                        surface_id.frame_sink_id());
    bool is_fallback_surface = it != frame_sink_ids_for_dependencies.end();
    if (!is_fallback_surface)
      continue;

    Surface* surface = surface_manager_->GetSurfaceForId(surface_id);
    // A misbehaving client may report a non-existent surface ID as a
    // |referenced_surface|. In that case, |surface| would be nullptr, and
    // there is nothing to do here.
    if (surface)
      surface->Close();
  }
}

void Surface::Close() {
  closed_ = true;
}

bool Surface::QueueFrame(cc::CompositorFrame frame,
                         uint64_t frame_index,
                         base::OnceClosure callback,
                         const WillDrawCallback& will_draw_callback) {
  late_activation_dependencies_.clear();

  if (frame.size_in_pixels() != surface_info_.size_in_pixels() ||
      frame.device_scale_factor() != surface_info_.device_scale_factor()) {
    TRACE_EVENT_INSTANT0("cc", "Surface invariants violation",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  if (closed_) {
    std::vector<ReturnedResource> resources =
        TransferableResource::ReturnResources(frame.resource_list);
    surface_client_->ReturnResources(resources);
    std::move(callback).Run();
    return true;
  }

  if (active_frame_data_ || pending_frame_data_)
    previous_frame_surface_id_ = surface_id();

  TakeLatencyInfoFromPendingFrame(&frame.metadata.latency_info);

  base::Optional<FrameData> previous_pending_frame_data =
      std::move(pending_frame_data_);
  pending_frame_data_.reset();

  UpdateActivationDependencies(frame);

  // Receive and track the resources referenced from the cc::CompositorFrame
  // regardless of whether it's pending or active.
  surface_client_->ReceiveFromChild(frame.resource_list);

  if (activation_dependencies_.empty()) {
    // If there are no blockers, then immediately activate the frame.
    ActivateFrame(FrameData(std::move(frame), frame_index, std::move(callback),
                            will_draw_callback));
  } else {
    pending_frame_data_ = FrameData(std::move(frame), frame_index,
                                    std::move(callback), will_draw_callback);

    RejectCompositorFramesToFallbackSurfaces();

    // Ask the surface manager to inform |this| when its dependencies are
    // resolved.
    surface_manager_->RequestSurfaceResolution(this);
  }

  // Returns resources for the previous pending frame.
  UnrefFrameResourcesAndRunDrawCallback(std::move(previous_pending_frame_data));

  return true;
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
  auto it = activation_dependencies_.find(surface_id);
  // This surface may no longer have blockers if the deadline has passed.
  if (it == activation_dependencies_.end())
    return;

  activation_dependencies_.erase(it);

  if (!activation_dependencies_.empty())
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
  late_activation_dependencies_ = std::move(activation_dependencies_);
  activation_dependencies_.clear();
  ActivatePendingFrame();
}

Surface::FrameData::FrameData(cc::CompositorFrame&& frame,
                              uint64_t frame_index,
                              base::OnceClosure draw_callback,
                              const WillDrawCallback& will_draw_callback)
    : frame(std::move(frame)),
      frame_index(frame_index),
      draw_callback(std::move(draw_callback)),
      will_draw_callback(will_draw_callback) {}

Surface::FrameData::FrameData(FrameData&& other) = default;

Surface::FrameData& Surface::FrameData::operator=(FrameData&& other) = default;

Surface::FrameData::~FrameData() = default;

void Surface::ActivatePendingFrame() {
  DCHECK(pending_frame_data_);
  FrameData frame_data = std::move(*pending_frame_data_);
  pending_frame_data_.reset();
  ActivateFrame(std::move(frame_data));
}

// A frame is activated if all its Surface ID dependences are active or a
// deadline has hit and the frame was forcibly activated.
void Surface::ActivateFrame(FrameData frame_data) {
  deadline_.Cancel();

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

  UnrefFrameResourcesAndRunDrawCallback(std::move(previous_frame_data));

  if (!seen_first_frame_activation_) {
    seen_first_frame_activation_ = true;
    surface_manager_->FirstSurfaceActivation(surface_info_);
  }

  if (surface_client_)
    surface_client_->OnSurfaceActivated(this);

  surface_manager_->SurfaceActivated(this);
}

void Surface::UpdateActivationDependencies(
    const cc::CompositorFrame& current_frame) {
  base::flat_set<SurfaceId> new_activation_dependencies;

  for (const SurfaceId& surface_id :
       current_frame.metadata.activation_dependencies) {
    Surface* dependency = surface_manager_->GetSurfaceForId(surface_id);
    // If a activation dependency does not have a corresponding active frame in
    // the display compositor, then it blocks this frame.
    if (!dependency || !dependency->HasActiveFrame())
      new_activation_dependencies.insert(surface_id);
  }

  // If this Surface has a previous pending frame, then we must determine the
  // changes in dependencies so that we can update the SurfaceDependencyTracker
  // map.
  base::flat_set<SurfaceId> added_dependencies;
  base::flat_set<SurfaceId> removed_dependencies;
  ComputeChangeInDependencies(activation_dependencies_,
                              new_activation_dependencies, &added_dependencies,
                              &removed_dependencies);

  // If there is a change in the dependency set, then inform SurfaceManager.
  if (!added_dependencies.empty() || !removed_dependencies.empty()) {
    surface_manager_->SurfaceDependenciesChanged(this, added_dependencies,
                                                 removed_dependencies);
  }

  activation_dependencies_ = std::move(new_activation_dependencies);
}

void Surface::ComputeChangeInDependencies(
    const base::flat_set<SurfaceId>& existing_dependencies,
    const base::flat_set<SurfaceId>& new_dependencies,
    base::flat_set<SurfaceId>* added_dependencies,
    base::flat_set<SurfaceId>* removed_dependencies) {
  for (const SurfaceId& surface_id : existing_dependencies) {
    if (!new_dependencies.count(surface_id))
      removed_dependencies->insert(surface_id);
  }

  for (const SurfaceId& surface_id : new_dependencies) {
    if (!existing_dependencies.count(surface_id))
      added_dependencies->insert(surface_id);
  }
}

void Surface::TakeCopyOutputRequests(Surface::CopyRequestsMap* copy_requests) {
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

const cc::CompositorFrame& Surface::GetActiveFrame() const {
  DCHECK(active_frame_data_);
  return active_frame_data_->frame;
}

const cc::CompositorFrame& Surface::GetPendingFrame() {
  DCHECK(pending_frame_data_);
  return pending_frame_data_->frame;
}

void Surface::TakeLatencyInfo(std::vector<ui::LatencyInfo>* latency_info) {
  if (!active_frame_data_)
    return;
  TakeLatencyInfoFromFrame(&active_frame_data_->frame, latency_info);
}

void Surface::RunDrawCallback() {
  if (active_frame_data_ && !active_frame_data_->draw_callback.is_null())
    std::move(active_frame_data_->draw_callback).Run();
}

void Surface::RunWillDrawCallback(const gfx::Rect& damage_rect) {
  if (!active_frame_data_ || active_frame_data_->will_draw_callback.is_null())
    return;

  active_frame_data_->will_draw_callback.Run(surface_id().local_surface_id(),
                                             damage_rect);
}

void Surface::AddDestructionDependency(SurfaceSequence sequence) {
  destruction_dependencies_.push_back(sequence);
}

void Surface::SatisfyDestructionDependencies(
    base::flat_set<SurfaceSequence>* sequences,
    base::flat_set<FrameSinkId>* valid_frame_sink_ids) {
  base::EraseIf(destruction_dependencies_,
                [sequences, valid_frame_sink_ids](SurfaceSequence seq) {
                  return (!!sequences->erase(seq) ||
                          !valid_frame_sink_ids->count(seq.frame_sink_id));
                });
}

void Surface::OnDeadline() {
  ActivatePendingFrameForDeadline();
}

void Surface::UnrefFrameResourcesAndRunDrawCallback(
    base::Optional<FrameData> frame_data) {
  if (!frame_data || !surface_client_)
    return;

  std::vector<ReturnedResource> resources =
      TransferableResource::ReturnResources(frame_data->frame.resource_list);
  // No point in returning same sync token to sender.
  for (auto& resource : resources)
    resource.sync_token.Clear();
  surface_client_->UnrefResources(resources);

  if (!frame_data->draw_callback.is_null())
    std::move(frame_data->draw_callback).Run();
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
    cc::CompositorFrame* frame,
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

}  // namespace viz
