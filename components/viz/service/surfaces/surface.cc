// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/surfaces/surface.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>

#include "base/stl_util.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
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
      deadline_(this, begin_frame_source),
      needs_sync_tokens_(needs_sync_tokens) {}

Surface::~Surface() {
  ClearCopyRequests();
  surface_manager_->SurfaceDiscarded(this);

  UnrefFrameResourcesAndRunCallbacks(std::move(pending_frame_data_));
  UnrefFrameResourcesAndRunCallbacks(std::move(active_frame_data_));

  deadline_.Cancel();
}

void Surface::Reset(base::WeakPtr<SurfaceClient> client) {
  seen_first_frame_activation_ = false;
  if (surface_client_.get() == client.get()) {
    UnrefFrameResourcesAndRunCallbacks(std::move(pending_frame_data_));
    UnrefFrameResourcesAndRunCallbacks(std::move(active_frame_data_));
  }
  surface_client_ = client;
  pending_frame_data_.reset();
  active_frame_data_.reset();
}

bool Surface::InheritActivationDeadlineFrom(
    const SurfaceDependencyDeadline& deadline) {
  TRACE_EVENT1("viz", "Surface::InheritActivationDeadlineFrom", "FrameSinkId",
               surface_id().frame_sink_id().ToString());
  return deadline_.InheritFrom(deadline);
}

void Surface::SetActivationDeadline(uint32_t number_of_frames_to_deadline) {
  TRACE_EVENT1("viz", "Surface::SetActivationDeadline", "FrameSinkId",
               surface_id().frame_sink_id().ToString());
  deadline_.Set(number_of_frames_to_deadline);
}

void Surface::SetPreviousFrameSurface(Surface* surface) {
  DCHECK(surface && (HasActiveFrame() || HasPendingFrame()));
  previous_frame_surface_id_ = surface->surface_id();
  CompositorFrame& frame = active_frame_data_ ? active_frame_data_->frame
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
  const std::vector<SurfaceId>& activation_dependencies =
      GetPendingFrame().metadata.activation_dependencies;

  for (const SurfaceId& surface_id :
       GetPendingFrame().metadata.referenced_surfaces) {
    // A surface ID in |referenced_surfaces| that has a corresponding surface
    // ID in |activation_dependencies| with the same frame sink ID is said to
    // be a fallback surface that can be used in place of the primary surface
    // if the deadline passes before the dependency becomes available.
    auto it = std::find_if(
        activation_dependencies.begin(), activation_dependencies.end(),
        [surface_id](const SurfaceId& dependency) {
          return dependency.frame_sink_id() == surface_id.frame_sink_id();
        });
    bool is_fallback_surface = it != activation_dependencies.end();
    if (!is_fallback_surface)
      continue;

    Surface* fallback_surface =
        surface_manager_->GetLatestInFlightSurface(*it, surface_id);

    // A misbehaving client may report a non-existent surface ID as a
    // |referenced_surface|. In that case, |surface| would be nullptr, and
    // there is nothing to do here.
    if (fallback_surface)
      fallback_surface->Close();
  }
}

void Surface::Close() {
  closed_ = true;
}

bool Surface::QueueFrame(
    CompositorFrame frame,
    uint64_t frame_index,
    base::OnceClosure callback,
    const AggregatedDamageCallback& aggregated_damage_callback,
    PresentedCallback presented_callback) {
  late_activation_dependencies_.clear();

  if (frame.size_in_pixels() != surface_info_.size_in_pixels() ||
      frame.device_scale_factor() != surface_info_.device_scale_factor()) {
    TRACE_EVENT_INSTANT0("cc", "Surface invariants violation",
                         TRACE_EVENT_SCOPE_THREAD);
    if (presented_callback) {
      std::move(presented_callback)
          .Run(base::TimeTicks(), base::TimeDelta(), 0);
    }
    return false;
  }

  if (closed_) {
    std::vector<ReturnedResource> resources =
        TransferableResource::ReturnResources(frame.resource_list);
    surface_client_->ReturnResources(resources);
    std::move(callback).Run();
    if (presented_callback) {
      std::move(presented_callback)
          .Run(base::TimeTicks(), base::TimeDelta(), 0);
    }
    return true;
  }

  if (active_frame_data_ || pending_frame_data_)
    previous_frame_surface_id_ = surface_id();

  TakeLatencyInfoFromPendingFrame(&frame.metadata.latency_info);

  base::Optional<FrameData> previous_pending_frame_data =
      std::move(pending_frame_data_);
  pending_frame_data_.reset();

  UpdateActivationDependencies(frame);

  // Receive and track the resources referenced from the CompositorFrame
  // regardless of whether it's pending or active.
  surface_client_->ReceiveFromChild(frame.resource_list);

  if (activation_dependencies_.empty()) {
    // If there are no blockers, then immediately activate the frame.
    ActivateFrame(FrameData(std::move(frame), frame_index, std::move(callback),
                            aggregated_damage_callback,
                            std::move(presented_callback)));
  } else {
    pending_frame_data_ =
        FrameData(std::move(frame), frame_index, std::move(callback),
                  aggregated_damage_callback, std::move(presented_callback));
    RejectCompositorFramesToFallbackSurfaces();

    base::Optional<uint32_t> deadline_in_frames =
        GetPendingFrame().metadata.deadline_in_frames;
    if (deadline_in_frames)
      deadline_.Set(*deadline_in_frames);

    // Ask the surface manager to inform |this| when its dependencies are
    // resolved.
    surface_manager_->RequestSurfaceResolution(this);
  }

  // Returns resources for the previous pending frame.
  UnrefFrameResourcesAndRunCallbacks(std::move(previous_pending_frame_data));

  return true;
}

void Surface::RequestCopyOfOutput(
    std::unique_ptr<CopyOutputRequest> copy_request) {
  if (!active_frame_data_)
    return;  // |copy_request| auto-sends empty result on out-of-scope.

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
  auto it = frame_sink_id_dependencies_.find(surface_id.frame_sink_id());
  if (it == frame_sink_id_dependencies_.end())
    return;

  if (surface_id.local_surface_id().parent_sequence_number() >=
          it->second.parent_sequence_number &&
      surface_id.local_surface_id().child_sequence_number() >=
          it->second.child_sequence_number) {
    frame_sink_id_dependencies_.erase(it);
    surface_manager_->SurfaceDependenciesChanged(this, {},
                                                 {surface_id.frame_sink_id()});
  }

  // LocalSurfaceIds of a given FrameSinkId are monotonically increasing in time
  // so if LocalSurfaceId j arrives then all LocalSurfaceIds i<j will never
  // arrive and so we just drop these invalid activation dependencies here.
  // TODO(fsamuel): This is a linear scan which is probably fine today because
  // a given surface has a small number of dependencies. We might need to
  // revisit this in the future if the number of dependencies grows
  // significantly.
  base::EraseIf(
      activation_dependencies_, [surface_id](const SurfaceId& dependency) {
        return dependency.frame_sink_id() == surface_id.frame_sink_id() &&
               dependency.local_surface_id() <= surface_id.local_surface_id();
      });

  if (!activation_dependencies_.empty())
    return;

  DCHECK(frame_sink_id_dependencies_.empty());

  // All blockers have been cleared. The surface can be activated now.
  ActivatePendingFrame();
}

void Surface::ActivatePendingFrameForDeadline() {
  if (!pending_frame_data_)
    return;

  // If a frame is being activated because of a deadline, then clear its set
  // of blockers.
  late_activation_dependencies_ = std::move(activation_dependencies_);
  activation_dependencies_.clear();
  frame_sink_id_dependencies_.clear();
  ActivatePendingFrame();
}

Surface::FrameData::FrameData(
    CompositorFrame&& frame,
    uint64_t frame_index,
    base::OnceClosure draw_callback,
    const AggregatedDamageCallback& aggregated_damage_callback,
    PresentedCallback presented_callback)
    : frame(std::move(frame)),
      frame_index(frame_index),
      draw_callback(std::move(draw_callback)),
      aggregated_damage_callback(aggregated_damage_callback),
      presented_callback(std::move(presented_callback)) {}

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
  TRACE_EVENT1("viz", "Surface::ActivateFrame", "FrameSinkId",
               surface_id().frame_sink_id().ToString());
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

  UnrefFrameResourcesAndRunCallbacks(std::move(previous_frame_data));

  if (!seen_first_frame_activation_) {
    seen_first_frame_activation_ = true;
    surface_manager_->FirstSurfaceActivation(surface_info_);
  }

  if (surface_client_)
    surface_client_->OnSurfaceActivated(this);

  surface_manager_->SurfaceActivated(this);
}

void Surface::UpdateActivationDependencies(
    const CompositorFrame& current_frame) {
  base::flat_map<FrameSinkId, SequenceNumbers> new_frame_sink_id_dependencies;
  base::flat_set<SurfaceId> new_activation_dependencies;

  for (const SurfaceId& surface_id :
       current_frame.metadata.activation_dependencies) {
    Surface* dependency = surface_manager_->GetSurfaceForId(surface_id);
    // If a activation dependency does not have a corresponding active frame in
    // the display compositor, then it blocks this frame.
    if (!dependency || !dependency->HasActiveFrame()) {
      // Record the latest |parent_sequence_number| this surface is interested
      // in observing for the provided FrameSinkId.
      uint32_t& parent_sequence_number =
          new_frame_sink_id_dependencies[surface_id.frame_sink_id()]
              .parent_sequence_number;
      parent_sequence_number =
          std::max(parent_sequence_number,
                   surface_id.local_surface_id().parent_sequence_number());

      uint32_t& child_sequence_number =
          new_frame_sink_id_dependencies[surface_id.frame_sink_id()]
              .child_sequence_number;
      child_sequence_number =
          std::max(child_sequence_number,
                   surface_id.local_surface_id().child_sequence_number());

      new_activation_dependencies.insert(surface_id);
    }
  }

  // If this Surface has a previous pending frame, then we must determine the
  // changes in dependencies so that we can update the SurfaceDependencyTracker
  // map.
  ComputeChangeInDependencies(new_frame_sink_id_dependencies);

  activation_dependencies_ = std::move(new_activation_dependencies);
  frame_sink_id_dependencies_ = std::move(new_frame_sink_id_dependencies);
}

void Surface::ComputeChangeInDependencies(
    const base::flat_map<FrameSinkId, SequenceNumbers>& new_dependencies) {
  base::flat_set<FrameSinkId> added_dependencies;
  base::flat_set<FrameSinkId> removed_dependencies;

  for (const auto& kv : frame_sink_id_dependencies_) {
    if (!new_dependencies.count(kv.first))
      removed_dependencies.insert(kv.first);
  }

  for (const auto& kv : new_dependencies) {
    if (!frame_sink_id_dependencies_.count(kv.first))
      added_dependencies.insert(kv.first);
  }

  // If there is a change in the dependency set, then inform SurfaceManager.
  if (!added_dependencies.empty() || !removed_dependencies.empty()) {
    surface_manager_->SurfaceDependenciesChanged(this, added_dependencies,
                                                 removed_dependencies);
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

bool Surface::TakePresentedCallback(PresentedCallback* callback) {
  if (active_frame_data_ && active_frame_data_->presented_callback) {
    *callback = std::move(active_frame_data_->presented_callback);
    return true;
  }
  return false;
}

void Surface::RunDrawCallback() {
  if (active_frame_data_ && !active_frame_data_->draw_callback.is_null())
    std::move(active_frame_data_->draw_callback).Run();
}

void Surface::NotifyAggregatedDamage(const gfx::Rect& damage_rect) {
  if (!active_frame_data_ ||
      active_frame_data_->aggregated_damage_callback.is_null())
    return;

  active_frame_data_->aggregated_damage_callback.Run(
      surface_id().local_surface_id(), damage_rect, active_frame_data_->frame);
}

void Surface::AddDestructionDependency(SurfaceSequence sequence) {
  destruction_dependencies_.push_back(sequence);
}

void Surface::SatisfyDestructionDependencies(
    base::flat_set<SurfaceSequence>* sequences,
    base::flat_map<FrameSinkId, std::string>* valid_frame_sink_ids) {
  base::EraseIf(destruction_dependencies_,
                [sequences, valid_frame_sink_ids](SurfaceSequence seq) {
                  return (!!sequences->erase(seq) ||
                          !valid_frame_sink_ids->count(seq.frame_sink_id));
                });
}

void Surface::OnDeadline() {
  TRACE_EVENT1("viz", "Surface::OnDeadline", "FrameSinkId",
               surface_id().frame_sink_id().ToString());
  ActivatePendingFrameForDeadline();
}

void Surface::UnrefFrameResourcesAndRunCallbacks(
    base::Optional<FrameData> frame_data) {
  if (!frame_data || !surface_client_)
    return;

  std::vector<ReturnedResource> resources =
      TransferableResource::ReturnResources(frame_data->frame.resource_list);
  // No point in returning same sync token to sender.
  for (auto& resource : resources)
    resource.sync_token.Clear();
  surface_client_->UnrefResources(resources);

  if (frame_data->draw_callback)
    std::move(frame_data->draw_callback).Run();

  if (frame_data->presented_callback) {
    std::move(frame_data->presented_callback)
        .Run(base::TimeTicks(), base::TimeDelta(), 0);
  }
}

void Surface::ClearCopyRequests() {
  if (active_frame_data_) {
    for (const auto& render_pass : active_frame_data_->frame.render_pass_list) {
      // When the container is cleared, all copy requests within it will
      // auto-send an empty result as they are being destroyed.
      render_pass->copy_requests.clear();
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

}  // namespace viz
