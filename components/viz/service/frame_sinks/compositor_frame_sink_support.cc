// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"

#include <algorithm>
#include <utility>

#include "cc/output/compositor_frame.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_reference.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/service/display/display.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support_client.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"

namespace viz {

// static
std::unique_ptr<CompositorFrameSinkSupport> CompositorFrameSinkSupport::Create(
    CompositorFrameSinkSupportClient* client,
    FrameSinkManagerImpl* frame_sink_manager,
    const FrameSinkId& frame_sink_id,
    bool is_root,
    bool handles_frame_sink_id_invalidation,
    bool needs_sync_tokens) {
  std::unique_ptr<CompositorFrameSinkSupport> support =
      base::WrapUnique(new CompositorFrameSinkSupport(
          client, frame_sink_id, is_root, handles_frame_sink_id_invalidation,
          needs_sync_tokens));
  support->Init(frame_sink_manager);
  return support;
}

CompositorFrameSinkSupport::~CompositorFrameSinkSupport() {
  if (!destruction_callback_.is_null())
    std::move(destruction_callback_).Run();

  // Unregister |this| as a cc::BeginFrameObserver so that the
  // cc::BeginFrameSource does not call into |this| after it's deleted.
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
  if (handles_frame_sink_id_invalidation_)
    surface_manager_->InvalidateFrameSinkId(frame_sink_id_);
}

void CompositorFrameSinkSupport::SetDestructionCallback(
    base::OnceCallback<void()> callback) {
  destruction_callback_ = std::move(callback);
}

void CompositorFrameSinkSupport::OnSurfaceActivated(cc::Surface* surface) {
  DCHECK(surface->HasActiveFrame());
  const cc::CompositorFrame& frame = surface->GetActiveFrame();
  if (!seen_first_frame_activation_) {
    // cc::SurfaceCreated only applies for the first cc::Surface activation.
    seen_first_frame_activation_ = true;

    gfx::Size frame_size = frame.render_pass_list.back()->output_rect.size();
    surface_manager_->SurfaceCreated(SurfaceInfo(
        surface->surface_id(), frame.metadata.device_scale_factor, frame_size));
  }
  // Fire cc::SurfaceCreated first so that a temporary reference is added before
  // it is potentially transformed into a real reference by the client.
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

void CompositorFrameSinkSupport::RefResources(
    const std::vector<cc::TransferableResource>& resources) {
  surface_resource_holder_.RefResources(resources);
}

void CompositorFrameSinkSupport::UnrefResources(
    const std::vector<cc::ReturnedResource>& resources) {
  surface_resource_holder_.UnrefResources(resources);
}

void CompositorFrameSinkSupport::ReturnResources(
    const std::vector<cc::ReturnedResource>& resources) {
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
    const std::vector<cc::TransferableResource>& resources) {
  surface_resource_holder_.ReceiveFromChild(resources);
}

void CompositorFrameSinkSupport::SetBeginFrameSource(
    cc::BeginFrameSource* begin_frame_source) {
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

void CompositorFrameSinkSupport::DidNotProduceFrame(
    const cc::BeginFrameAck& ack) {
  TRACE_EVENT2("cc", "CompositorFrameSinkSupport::DidNotProduceFrame",
               "ack.source_id", ack.source_id, "ack.sequence_number",
               ack.sequence_number);
  DCHECK_GE(ack.sequence_number, cc::BeginFrameArgs::kStartingFrameNumber);

  // |has_damage| is not transmitted, but false by default.
  DCHECK(!ack.has_damage);

  if (current_surface_id_.is_valid())
    surface_manager_->SurfaceModified(current_surface_id_, ack);

  if (begin_frame_source_)
    begin_frame_source_->DidFinishFrame(this);
}

bool CompositorFrameSinkSupport::SubmitCompositorFrame(
    const LocalSurfaceId& local_surface_id,
    cc::CompositorFrame frame) {
  TRACE_EVENT0("cc", "CompositorFrameSinkSupport::SubmitCompositorFrame");
  DCHECK(local_surface_id.is_valid());
  DCHECK(!frame.render_pass_list.empty());

  ++ack_pending_count_;

  // |has_damage| is not transmitted.
  frame.metadata.begin_frame_ack.has_damage = true;
  cc::BeginFrameAck ack = frame.metadata.begin_frame_ack;
  DCHECK_LE(cc::BeginFrameArgs::kStartingFrameNumber, ack.sequence_number);

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

  cc::Surface* prev_surface =
      surface_manager_->GetSurfaceForId(current_surface_id_);
  cc::Surface* current_surface = nullptr;
  if (prev_surface &&
      local_surface_id == current_surface_id_.local_surface_id()) {
    current_surface = prev_surface;
  } else {
    SurfaceId surface_id(frame_sink_id_, local_surface_id);
    gfx::Size frame_size = frame.render_pass_list.back()->output_rect.size();
    float device_scale_factor = frame.metadata.device_scale_factor;
    SurfaceInfo surface_info(surface_id, device_scale_factor, frame_size);

    if (!surface_info.is_valid()) {
      TRACE_EVENT_INSTANT0("cc", "Invalid SurfaceInfo",
                           TRACE_EVENT_SCOPE_THREAD);
      EvictCurrentSurface();
      std::vector<cc::ReturnedResource> resources =
          cc::TransferableResource::ReturnResources(frame.resource_list);
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
      std::move(frame),
      base::Bind(&CompositorFrameSinkSupport::DidReceiveCompositorFrameAck,
                 weak_factory_.GetWeakPtr()),
      base::BindRepeating(&CompositorFrameSinkSupport::WillDrawSurface,
                          weak_factory_.GetWeakPtr()));

  if (!result) {
    EvictCurrentSurface();
    return false;
  }

  if (prev_surface && prev_surface != current_surface) {
    current_surface->SetPreviousFrameSurface(prev_surface);
    surface_manager_->DestroySurface(prev_surface->surface_id());
  }

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
  std::vector<cc::SurfaceReference> references_to_add;
  std::vector<cc::SurfaceReference> references_to_remove;
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

  // Modify surface references stored in cc::SurfaceManager.
  if (!references_to_add.empty())
    surface_manager_->AddSurfaceReferences(references_to_add);
  if (!references_to_remove.empty())
    surface_manager_->RemoveSurfaceReferences(references_to_remove);
}

cc::SurfaceReference CompositorFrameSinkSupport::MakeTopLevelRootReference(
    const SurfaceId& surface_id) {
  return cc::SurfaceReference(surface_manager_->GetRootSurfaceId(), surface_id);
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

CompositorFrameSinkSupport::CompositorFrameSinkSupport(
    CompositorFrameSinkSupportClient* client,
    const FrameSinkId& frame_sink_id,
    bool is_root,
    bool handles_frame_sink_id_invalidation,
    bool needs_sync_tokens)
    : client_(client),
      frame_sink_id_(frame_sink_id),
      surface_resource_holder_(this),
      is_root_(is_root),
      needs_sync_tokens_(needs_sync_tokens),
      handles_frame_sink_id_invalidation_(handles_frame_sink_id_invalidation),
      weak_factory_(this) {}

void CompositorFrameSinkSupport::Init(
    FrameSinkManagerImpl* frame_sink_manager) {
  frame_sink_manager_ = frame_sink_manager;
  surface_manager_ = frame_sink_manager->surface_manager();
  if (handles_frame_sink_id_invalidation_)
    surface_manager_->RegisterFrameSinkId(frame_sink_id_);
  frame_sink_manager_->RegisterFrameSinkManagerClient(frame_sink_id_, this);
}

void CompositorFrameSinkSupport::OnBeginFrame(const cc::BeginFrameArgs& args) {
  UpdateNeedsBeginFramesInternal();
  if (current_surface_id_.is_valid()) {
    surface_manager_->SurfaceDamageExpected(current_surface_id_, args);
  }
  last_begin_frame_args_ = args;
  if (client_)
    client_->OnBeginFrame(args);
}

const cc::BeginFrameArgs& CompositorFrameSinkSupport::LastUsedBeginFrameArgs()
    const {
  return last_begin_frame_args_;
}

void CompositorFrameSinkSupport::OnBeginFrameSourcePausedChanged(bool paused) {}

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

cc::Surface* CompositorFrameSinkSupport::CreateSurface(
    const SurfaceInfo& surface_info) {
  seen_first_frame_activation_ = false;
  return surface_manager_->CreateSurface(
      weak_factory_.GetWeakPtr(), surface_info,
      frame_sink_manager_->GetPrimaryBeginFrameSource(), needs_sync_tokens_);
}

void CompositorFrameSinkSupport::RequestCopyOfSurface(
    std::unique_ptr<cc::CopyOutputRequest> copy_request) {
  if (!current_surface_id_.is_valid())
    return;
  cc::Surface* current_surface =
      surface_manager_->GetSurfaceForId(current_surface_id_);
  current_surface->RequestCopyOfOutput(std::move(copy_request));
  cc::BeginFrameAck ack;
  ack.has_damage = true;
  if (current_surface->HasActiveFrame())
    surface_manager_->SurfaceModified(current_surface->surface_id(), ack);
}

cc::Surface* CompositorFrameSinkSupport::GetCurrentSurfaceForTesting() {
  return surface_manager_->GetSurfaceForId(current_surface_id_);
}

}  // namespace viz
