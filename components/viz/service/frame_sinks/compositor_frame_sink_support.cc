// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"

#include <algorithm>
#include <utility>

#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/service/display/display.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_reference.h"

namespace viz {

// static
std::unique_ptr<CompositorFrameSinkSupport> CompositorFrameSinkSupport::Create(
    mojom::CompositorFrameSinkClient* client,
    FrameSinkManagerImpl* frame_sink_manager,
    const FrameSinkId& frame_sink_id,
    bool is_root,
    bool needs_sync_tokens) {
  std::unique_ptr<CompositorFrameSinkSupport> support =
      base::WrapUnique(new CompositorFrameSinkSupport(
          client, frame_sink_id, is_root, needs_sync_tokens));
  support->Init(frame_sink_manager);
  return support;
}

CompositorFrameSinkSupport::~CompositorFrameSinkSupport() {
  if (!destruction_callback_.is_null())
    std::move(destruction_callback_).Run();

  // Unregister |this| as a BeginFrameObserver so that the
  // BeginFrameSource does not call into |this| after it's deleted.
  SetNeedsBeginFrame(false);

  // For display root surfaces the surface is no longer going to be visible.
  // Make it unreachable from the top-level root.
  if (referenced_local_surface_id_.has_value()) {
    auto reference = MakeTopLevelRootReference(
        SurfaceId(frame_sink_id_, referenced_local_surface_id_.value()));
    surface_manager_->RemoveSurfaceReferences({reference});
  }

  EvictCurrentSurface();
  frame_sink_manager_->UnregisterFrameSinkManagerClient(frame_sink_id_);
}

void CompositorFrameSinkSupport::SetAggregatedDamageCallback(
    AggregatedDamageCallback callback) {
  aggregated_damage_callback_ = std::move(callback);
}

void CompositorFrameSinkSupport::SetDestructionCallback(
    base::OnceCallback<void()> callback) {
  destruction_callback_ = std::move(callback);
}

void CompositorFrameSinkSupport::OnSurfaceActivated(Surface* surface) {
  DCHECK(surface);
  DCHECK(surface->HasActiveFrame());
  DCHECK(surface->active_referenced_surfaces());
  UpdateSurfaceReferences(surface->surface_id().local_surface_id(),
                          *surface->active_referenced_surfaces());
}

void CompositorFrameSinkSupport::RefResources(
    const std::vector<TransferableResource>& resources) {
  surface_resource_holder_.RefResources(resources);
}

void CompositorFrameSinkSupport::UnrefResources(
    const std::vector<ReturnedResource>& resources) {
  surface_resource_holder_.UnrefResources(resources);
}

void CompositorFrameSinkSupport::ReturnResources(
    const std::vector<ReturnedResource>& resources) {
  if (resources.empty())
    return;
  if (!ack_pending_count_ && client_) {
    client_->ReclaimResources(resources);
    return;
  }

  std::copy(resources.begin(), resources.end(),
            std::back_inserter(surface_returned_resources_));
}

void CompositorFrameSinkSupport::ReceiveFromChild(
    const std::vector<TransferableResource>& resources) {
  surface_resource_holder_.ReceiveFromChild(resources);
}

void CompositorFrameSinkSupport::SetBeginFrameSource(
    BeginFrameSource* begin_frame_source) {
  if (begin_frame_source_ && added_frame_observer_) {
    begin_frame_source_->RemoveObserver(this);
    added_frame_observer_ = false;
  }
  begin_frame_source_ = begin_frame_source;
  UpdateNeedsBeginFramesInternal();
}

void CompositorFrameSinkSupport::EvictCurrentSurface() {
  if (!current_surface_id_.is_valid())
    return;
  SurfaceId to_destroy_surface_id = current_surface_id_;
  current_surface_id_ = SurfaceId();
  surface_manager_->DestroySurface(to_destroy_surface_id);
}

void CompositorFrameSinkSupport::SetNeedsBeginFrame(bool needs_begin_frame) {
  needs_begin_frame_ = needs_begin_frame;
  UpdateNeedsBeginFramesInternal();
}

void CompositorFrameSinkSupport::DidNotProduceFrame(const BeginFrameAck& ack) {
  TRACE_EVENT2("cc", "CompositorFrameSinkSupport::DidNotProduceFrame",
               "ack.source_id", ack.source_id, "ack.sequence_number",
               ack.sequence_number);
  DCHECK_GE(ack.sequence_number, BeginFrameArgs::kStartingFrameNumber);

  // |has_damage| is not transmitted, but false by default.
  DCHECK(!ack.has_damage);

  if (current_surface_id_.is_valid())
    surface_manager_->SurfaceModified(current_surface_id_, ack);

  if (begin_frame_source_)
    begin_frame_source_->DidFinishFrame(this);
}

void CompositorFrameSinkSupport::SubmitCompositorFrame(
    const LocalSurfaceId& local_surface_id,
    CompositorFrame frame,
    mojom::HitTestRegionListPtr hit_test_region_list,
    uint64_t submit_time) {
  SubmitCompositorFrame(local_surface_id, std::move(frame),
                        std::move(hit_test_region_list));
}

bool CompositorFrameSinkSupport::SubmitCompositorFrame(
    const LocalSurfaceId& local_surface_id,
    CompositorFrame frame,
    mojom::HitTestRegionListPtr hit_test_region_list) {
  TRACE_EVENT0("cc", "CompositorFrameSinkSupport::SubmitCompositorFrame");
  DCHECK(local_surface_id.is_valid());
  DCHECK(!frame.render_pass_list.empty());

  uint64_t frame_index = ++last_frame_index_;
  ++ack_pending_count_;

  // |has_damage| is not transmitted.
  frame.metadata.begin_frame_ack.has_damage = true;
  BeginFrameAck ack = frame.metadata.begin_frame_ack;
  DCHECK_LE(BeginFrameArgs::kStartingFrameNumber, ack.sequence_number);

  if (!ui::LatencyInfo::Verify(frame.metadata.latency_info,
                               "RenderWidgetHostImpl::OnSwapCompositorFrame")) {
    std::vector<ui::LatencyInfo>().swap(frame.metadata.latency_info);
  }
  for (ui::LatencyInfo& latency : frame.metadata.latency_info) {
    if (latency.latency_components().size() > 0) {
      latency.AddLatencyNumber(ui::DISPLAY_COMPOSITOR_RECEIVED_FRAME_COMPONENT,
                               0, 0);
    }
  }

  Surface* prev_surface =
      surface_manager_->GetSurfaceForId(current_surface_id_);
  Surface* current_surface = nullptr;
  if (prev_surface &&
      local_surface_id == current_surface_id_.local_surface_id()) {
    current_surface = prev_surface;
  } else {
    SurfaceId surface_id(frame_sink_id_, local_surface_id);
    SurfaceInfo surface_info(surface_id, frame.device_scale_factor(),
                             frame.size_in_pixels());

    if (!surface_info.is_valid()) {
      TRACE_EVENT_INSTANT0("cc", "Invalid SurfaceInfo",
                           TRACE_EVENT_SCOPE_THREAD);
      EvictCurrentSurface();
      std::vector<ReturnedResource> resources =
          TransferableResource::ReturnResources(frame.resource_list);
      ReturnResources(resources);
      DidReceiveCompositorFrameAck();
      return true;
    }

    current_surface = CreateSurface(surface_info);
    current_surface_id_ = SurfaceId(frame_sink_id_, local_surface_id);
    surface_manager_->SurfaceDamageExpected(current_surface->surface_id(),
                                            last_begin_frame_args_);
  }

  bool result = current_surface->QueueFrame(
      std::move(frame), frame_index,
      base::BindOnce(&CompositorFrameSinkSupport::DidReceiveCompositorFrameAck,
                     weak_factory_.GetWeakPtr()),
      aggregated_damage_callback_);

  if (!result) {
    EvictCurrentSurface();
    return false;
  }

  if (prev_surface && prev_surface != current_surface) {
    current_surface->SetPreviousFrameSurface(prev_surface);
    surface_manager_->DestroySurface(prev_surface->surface_id());
  }

  frame_sink_manager()->SubmitHitTestRegionList(
      current_surface_id_, frame_index, std::move(hit_test_region_list));

  if (begin_frame_source_)
    begin_frame_source_->DidFinishFrame(this);

  return true;
}

void CompositorFrameSinkSupport::UpdateSurfaceReferences(
    const LocalSurfaceId& local_surface_id,
    const std::vector<SurfaceId>& active_referenced_surfaces) {
  SurfaceId surface_id(frame_sink_id_, local_surface_id);

  const base::flat_set<SurfaceId>& existing_referenced_surfaces =
      surface_manager_->GetSurfacesReferencedByParent(surface_id);

  base::flat_set<SurfaceId> new_referenced_surfaces(
      active_referenced_surfaces.begin(), active_referenced_surfaces.end(),
      base::KEEP_FIRST_OF_DUPES);

  // Populate list of surface references to add and remove by getting the
  // difference between existing surface references and surface references for
  // latest activated CompositorFrame.
  std::vector<SurfaceReference> references_to_add;
  std::vector<SurfaceReference> references_to_remove;
  GetSurfaceReferenceDifference(surface_id, existing_referenced_surfaces,
                                new_referenced_surfaces, &references_to_add,
                                &references_to_remove);

  // Check if this is a display root surface and the SurfaceId is changing.
  if (is_root_ && (!referenced_local_surface_id_.has_value() ||
                   referenced_local_surface_id_.value() != local_surface_id)) {
    // Make the new SurfaceId reachable from the top-level root.
    references_to_add.push_back(MakeTopLevelRootReference(surface_id));

    // Make the old SurfaceId unreachable from the top-level root if applicable.
    if (referenced_local_surface_id_.has_value()) {
      references_to_remove.push_back(MakeTopLevelRootReference(
          SurfaceId(frame_sink_id_, referenced_local_surface_id_.value())));
    }

    referenced_local_surface_id_ = local_surface_id;
  }

  // Modify surface references stored in SurfaceManager.
  if (!references_to_add.empty())
    surface_manager_->AddSurfaceReferences(references_to_add);
  if (!references_to_remove.empty())
    surface_manager_->RemoveSurfaceReferences(references_to_remove);
}

SurfaceReference CompositorFrameSinkSupport::MakeTopLevelRootReference(
    const SurfaceId& surface_id) {
  return SurfaceReference(surface_manager_->GetRootSurfaceId(), surface_id);
}

void CompositorFrameSinkSupport::DidReceiveCompositorFrameAck() {
  DCHECK_GT(ack_pending_count_, 0);
  ack_pending_count_--;
  if (!client_)
    return;

  client_->DidReceiveCompositorFrameAck(surface_returned_resources_);
  surface_returned_resources_.clear();
}

CompositorFrameSinkSupport::CompositorFrameSinkSupport(
    mojom::CompositorFrameSinkClient* client,
    const FrameSinkId& frame_sink_id,
    bool is_root,
    bool needs_sync_tokens)
    : client_(client),
      frame_sink_id_(frame_sink_id),
      surface_resource_holder_(this),
      is_root_(is_root),
      needs_sync_tokens_(needs_sync_tokens),
      weak_factory_(this) {}

void CompositorFrameSinkSupport::Init(
    FrameSinkManagerImpl* frame_sink_manager) {
  frame_sink_manager_ = frame_sink_manager;
  surface_manager_ = frame_sink_manager->surface_manager();
  frame_sink_manager_->RegisterFrameSinkManagerClient(frame_sink_id_, this);
}

void CompositorFrameSinkSupport::OnBeginFrame(const BeginFrameArgs& args) {
  UpdateNeedsBeginFramesInternal();
  if (current_surface_id_.is_valid())
    surface_manager_->SurfaceDamageExpected(current_surface_id_, args);
  last_begin_frame_args_ = args;
  if (client_)
    client_->OnBeginFrame(args);
}

const BeginFrameArgs& CompositorFrameSinkSupport::LastUsedBeginFrameArgs()
    const {
  return last_begin_frame_args_;
}

void CompositorFrameSinkSupport::OnBeginFrameSourcePausedChanged(bool paused) {
  if (client_)
    client_->OnBeginFramePausedChanged(paused);
}

void CompositorFrameSinkSupport::UpdateNeedsBeginFramesInternal() {
  if (!begin_frame_source_)
    return;

  if (needs_begin_frame_ == added_frame_observer_)
    return;

  added_frame_observer_ = needs_begin_frame_;
  if (needs_begin_frame_)
    begin_frame_source_->AddObserver(this);
  else
    begin_frame_source_->RemoveObserver(this);
}

Surface* CompositorFrameSinkSupport::CreateSurface(
    const SurfaceInfo& surface_info) {
  return surface_manager_->CreateSurface(
      weak_factory_.GetWeakPtr(), surface_info,
      frame_sink_manager_->GetPrimaryBeginFrameSource(), needs_sync_tokens_);
}

void CompositorFrameSinkSupport::RequestCopyOfSurface(
    std::unique_ptr<CopyOutputRequest> copy_request) {
  if (!current_surface_id_.is_valid())
    return;
  Surface* current_surface =
      surface_manager_->GetSurfaceForId(current_surface_id_);
  current_surface->RequestCopyOfOutput(std::move(copy_request));
  BeginFrameAck ack;
  ack.has_damage = true;
  if (current_surface->HasActiveFrame())
    surface_manager_->SurfaceModified(current_surface->surface_id(), ack);
}

Surface* CompositorFrameSinkSupport::GetCurrentSurfaceForTesting() {
  return surface_manager_->GetSurfaceForId(current_surface_id_);
}

}  // namespace viz
