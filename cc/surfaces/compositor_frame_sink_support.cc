// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/compositor_frame_sink_support.h"

#include <algorithm>
#include <utility>

#include "cc/output/compositor_frame.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/surfaces/compositor_frame_sink_support_client.h"
#include "cc/surfaces/display.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_info.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/surfaces/surface_reference.h"

namespace cc {

// static
std::unique_ptr<CompositorFrameSinkSupport> CompositorFrameSinkSupport::Create(
    CompositorFrameSinkSupportClient* client,
    SurfaceManager* surface_manager,
    const FrameSinkId& frame_sink_id,
    bool is_root,
    bool handles_frame_sink_id_invalidation,
    bool needs_sync_points) {
  std::unique_ptr<CompositorFrameSinkSupport> support =
      base::WrapUnique(new CompositorFrameSinkSupport(
          client, frame_sink_id, is_root, handles_frame_sink_id_invalidation,
          needs_sync_points));
  support->Init(surface_manager);
  return support;
}

CompositorFrameSinkSupport::~CompositorFrameSinkSupport() {
  // Unregister |this| as a BeginFrameObserver so that the BeginFrameSource does
  // not call into |this| after it's deleted.
  SetNeedsBeginFrame(false);

  // For display root surfaces the surface is no longer going to be visible.
  // Make it unreachable from the top-level root.
  if (referenced_local_surface_id_.has_value()) {
    auto reference = MakeTopLevelRootReference(
        SurfaceId(frame_sink_id_, referenced_local_surface_id_.value()));
    surface_manager_->RemoveSurfaceReferences({reference});
  }

  EvictCurrentSurface();
  surface_manager_->UnregisterFrameSinkManagerClient(frame_sink_id_);
  if (handles_frame_sink_id_invalidation_)
    surface_manager_->InvalidateFrameSinkId(frame_sink_id_);
}

void CompositorFrameSinkSupport::ReturnResources(
    const ReturnedResourceArray& resources) {
  if (resources.empty())
    return;
  if (!ack_pending_count_ && client_) {
    client_->ReclaimResources(resources);
    return;
  }

  std::copy(resources.begin(), resources.end(),
            std::back_inserter(surface_returned_resources_));
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
  if (!current_surface_)
    return;
  DestroyCurrentSurface();
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

  if (current_surface_)
    surface_manager_->SurfaceModified(current_surface_->surface_id(), ack);
  if (begin_frame_source_)
    begin_frame_source_->DidFinishFrame(this);
}

bool CompositorFrameSinkSupport::SubmitCompositorFrame(
    const LocalSurfaceId& local_surface_id,
    CompositorFrame frame) {
  TRACE_EVENT0("cc", "CompositorFrameSinkSupport::SubmitCompositorFrame");
  DCHECK(local_surface_id.is_valid());
  DCHECK(!frame.render_pass_list.empty());

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

  std::unique_ptr<Surface> surface;
  bool create_new_surface =
      (!current_surface_ ||
       local_surface_id != current_surface_->surface_id().local_surface_id());
  if (!create_new_surface) {
    surface = std::move(current_surface_);
  } else {
    SurfaceId surface_id(frame_sink_id_, local_surface_id);
    gfx::Size frame_size = frame.render_pass_list.back()->output_rect.size();
    float device_scale_factor = frame.metadata.device_scale_factor;
    SurfaceInfo surface_info(surface_id, device_scale_factor, frame_size);

    if (!surface_info.is_valid()) {
      TRACE_EVENT_INSTANT0("cc", "Invalid SurfaceInfo",
                           TRACE_EVENT_SCOPE_THREAD);
      if (current_surface_)
        DestroyCurrentSurface();
      ReturnedResourceArray resources;
      TransferableResource::ReturnResources(frame.resource_list, &resources);
      ReturnResources(resources);
      DidReceiveCompositorFrameAck();
      return true;
    }

    surface = CreateSurface(surface_info);
    surface_manager_->SurfaceDamageExpected(surface->surface_id(),
                                            last_begin_frame_args_);
  }

  bool result = surface->QueueFrame(
      std::move(frame),
      base::Bind(&CompositorFrameSinkSupport::DidReceiveCompositorFrameAck,
                 weak_factory_.GetWeakPtr()),
      base::BindRepeating(&CompositorFrameSinkSupport::WillDrawSurface,
                          weak_factory_.GetWeakPtr()));

  if (!result) {
    surface_manager_->DestroySurface(std::move(surface));
    return false;
  }

  if (current_surface_) {
    surface->SetPreviousFrameSurface(current_surface_.get());
    DestroyCurrentSurface();
  }
  current_surface_ = std::move(surface);

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

void CompositorFrameSinkSupport::WillDrawSurface(
    const LocalSurfaceId& local_surface_id,
    const gfx::Rect& damage_rect) {
  if (client_)
    client_->WillDrawSurface(local_surface_id, damage_rect);
}

void CompositorFrameSinkSupport::ClaimTemporaryReference(
    const SurfaceId& surface_id) {
  surface_manager_->AssignTemporaryReference(surface_id, frame_sink_id_);
}

void CompositorFrameSinkSupport::ReceiveFromChild(
    const TransferableResourceArray& resources) {
  surface_resource_holder_.ReceiveFromChild(resources);
}

void CompositorFrameSinkSupport::RefResources(
    const TransferableResourceArray& resources) {
  surface_resource_holder_.RefResources(resources);
}

void CompositorFrameSinkSupport::UnrefResources(
    const ReturnedResourceArray& resources) {
  surface_resource_holder_.UnrefResources(resources);
}

CompositorFrameSinkSupport::CompositorFrameSinkSupport(
    CompositorFrameSinkSupportClient* client,
    const FrameSinkId& frame_sink_id,
    bool is_root,
    bool handles_frame_sink_id_invalidation,
    bool needs_sync_points)
    : client_(client),
      frame_sink_id_(frame_sink_id),
      surface_resource_holder_(this),
      is_root_(is_root),
      needs_sync_points_(needs_sync_points),
      handles_frame_sink_id_invalidation_(handles_frame_sink_id_invalidation),
      weak_factory_(this) {}

void CompositorFrameSinkSupport::Init(SurfaceManager* surface_manager) {
  surface_manager_ = surface_manager;
  if (handles_frame_sink_id_invalidation_)
    surface_manager_->RegisterFrameSinkId(frame_sink_id_);
  surface_manager_->RegisterFrameSinkManagerClient(frame_sink_id_, this);
}

void CompositorFrameSinkSupport::OnBeginFrame(const BeginFrameArgs& args) {
  UpdateNeedsBeginFramesInternal();
  if (current_surface_) {
    surface_manager_->SurfaceDamageExpected(current_surface_->surface_id(),
                                            args);
  }
  last_begin_frame_args_ = args;
  if (client_)
    client_->OnBeginFrame(args);
}

const BeginFrameArgs& CompositorFrameSinkSupport::LastUsedBeginFrameArgs()
    const {
  return last_begin_frame_args_;
}

void CompositorFrameSinkSupport::OnBeginFrameSourcePausedChanged(bool paused) {}

void CompositorFrameSinkSupport::OnSurfaceActivated(Surface* surface) {
  DCHECK(surface->HasActiveFrame());
  const CompositorFrame& frame = surface->GetActiveFrame();
  if (!seen_first_frame_activation_) {
    // SurfaceCreated only applies for the first Surface activation.
    seen_first_frame_activation_ = true;

    gfx::Size frame_size = frame.render_pass_list.back()->output_rect.size();
    surface_manager_->SurfaceCreated(SurfaceInfo(
        surface->surface_id(), frame.metadata.device_scale_factor, frame_size));
  }
  // Fire SurfaceCreated first so that a temporary reference is added before it
  // is potentially transformed into a real reference by the client.
  DCHECK(surface->active_referenced_surfaces());
  UpdateSurfaceReferences(surface->surface_id().local_surface_id(),
                          *surface->active_referenced_surfaces());
  if (!surface_manager_->SurfaceModified(surface->surface_id(),
                                         frame.metadata.begin_frame_ack)) {
    TRACE_EVENT_INSTANT0("cc", "Damage not visible.", TRACE_EVENT_SCOPE_THREAD);
    surface->RunDrawCallback();
  }
  surface_manager_->SurfaceActivated(surface);
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

std::unique_ptr<Surface> CompositorFrameSinkSupport::CreateSurface(
    const SurfaceInfo& surface_info) {
  seen_first_frame_activation_ = false;
  return surface_manager_->CreateSurface(weak_factory_.GetWeakPtr(),
                                         surface_info);
}

void CompositorFrameSinkSupport::DestroyCurrentSurface() {
  surface_manager_->DestroySurface(std::move(current_surface_));
}

void CompositorFrameSinkSupport::RequestCopyOfSurface(
    std::unique_ptr<CopyOutputRequest> copy_request) {
  if (!current_surface_)
    return;

  DCHECK(current_surface_->compositor_frame_sink_support().get() == this);
  current_surface_->RequestCopyOfOutput(std::move(copy_request));
  BeginFrameAck ack;
  ack.has_damage = true;
  if (current_surface_->HasActiveFrame())
    surface_manager_->SurfaceModified(current_surface_->surface_id(), ack);
}

}  // namespace cc
