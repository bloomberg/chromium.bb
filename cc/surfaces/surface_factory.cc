// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/surface_factory.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/copy_output_request.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_factory_client.h"
#include "cc/surfaces/surface_info.h"
#include "cc/surfaces/surface_manager.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
SurfaceFactory::SurfaceFactory(
    const FrameSinkId& frame_sink_id,
    SurfaceManager* manager,
    SurfaceFactoryClient* client,
    SurfaceResourceHolderClient* resource_holder_client)
    : frame_sink_id_(frame_sink_id),
      manager_(manager),
      client_(client),
      holder_(resource_holder_client),
      needs_sync_points_(true),
      weak_factory_(this) {}

SurfaceFactory::~SurfaceFactory() {
  // This is to prevent troubles when a factory that resides in a client is
  // being destroyed. In such cases, the factory might attempt to return
  // resources to the client while it's in the middle of destruction and this
  // could cause a crash or some unexpected behaviour.
  DCHECK(!current_surface_) << "Please call EvictSurface before destruction";
}

void SurfaceFactory::EvictSurface() {
  if (!current_surface_)
    return;
  Destroy(std::move(current_surface_));
}

void SurfaceFactory::SubmitCompositorFrame(
    const LocalSurfaceId& local_surface_id,
    CompositorFrame frame,
    const DrawCallback& callback,
    const WillDrawCallback& will_draw_callback) {
  TRACE_EVENT0("cc", "SurfaceFactory::SubmitCompositorFrame");
  DCHECK(local_surface_id.is_valid());

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
    surface = Create(local_surface_id);
  }
  surface->QueueFrame(std::move(frame), callback, will_draw_callback);

  if (current_surface_ && create_new_surface) {
    surface->SetPreviousFrameSurface(current_surface_.get());
    Destroy(std::move(current_surface_));
  }
  current_surface_ = std::move(surface);
}

void SurfaceFactory::RequestCopyOfSurface(
    std::unique_ptr<CopyOutputRequest> copy_request) {
  if (!current_surface_) {
    copy_request->SendEmptyResult();
    return;
  }
  DCHECK(current_surface_->factory().get() == this);
  current_surface_->RequestCopyOfOutput(std::move(copy_request));
  manager_->SurfaceModified(current_surface_->surface_id());
}

void SurfaceFactory::ReceiveFromChild(
    const TransferableResourceArray& resources) {
  holder_.ReceiveFromChild(resources);
}

void SurfaceFactory::RefResources(const TransferableResourceArray& resources) {
  holder_.RefResources(resources);
}

void SurfaceFactory::UnrefResources(const ReturnedResourceArray& resources) {
  holder_.UnrefResources(resources);
}

void SurfaceFactory::OnSurfaceActivated(Surface* surface) {
  DCHECK(surface->HasActiveFrame());
  if (!seen_first_frame_activation_) {
    seen_first_frame_activation_ = true;

    const CompositorFrame& frame = surface->GetActiveFrame();
    // CompositorFrames might not be populated with a RenderPass in unit tests.
    gfx::Size frame_size;
    if (!frame.render_pass_list.empty())
      frame_size = frame.render_pass_list.back()->output_rect.size();

    // SurfaceCreated only applies for the first Surface activation. Thus,
    // SurfaceFactory stops observing new activations after the first one.
    manager_->SurfaceCreated(SurfaceInfo(
        surface->surface_id(), frame.metadata.device_scale_factor, frame_size));
  }
  // Fire SurfaceCreated first so that a temporary reference is added before it
  // is potentially transformed into a real reference by the client.
  client_->ReferencedSurfacesChanged(surface->surface_id().local_surface_id(),
                                     surface->active_referenced_surfaces());
  if (!manager_->SurfaceModified(surface->surface_id())) {
    TRACE_EVENT_INSTANT0("cc", "Damage not visible.", TRACE_EVENT_SCOPE_THREAD);
    surface->RunDrawCallback();
  }
}

void SurfaceFactory::OnSurfaceDependenciesChanged(
    Surface* surface,
    const SurfaceDependencies& added_dependencies,
    const SurfaceDependencies& removed_dependencies) {}

void SurfaceFactory::OnSurfaceDiscarded(Surface* surface) {}

std::unique_ptr<Surface> SurfaceFactory::Create(
    const LocalSurfaceId& local_surface_id) {
  seen_first_frame_activation_ = false;
  std::unique_ptr<Surface> surface =
      manager_->CreateSurface(weak_factory_.GetWeakPtr(), local_surface_id);
  surface->AddObserver(this);
  return surface;
}

void SurfaceFactory::Destroy(std::unique_ptr<Surface> surface) {
  surface->RemoveObserver(this);
  manager_->DestroySurface(std::move(surface));
}

}  // namespace cc
