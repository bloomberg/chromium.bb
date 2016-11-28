// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/surface_factory.h"

#include <utility>

#include "base/trace_event/trace_event.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/copy_output_request.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_factory_client.h"
#include "cc/surfaces/surface_manager.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
SurfaceFactory::SurfaceFactory(const FrameSinkId& frame_sink_id,
                               SurfaceManager* manager,
                               SurfaceFactoryClient* client)
    : frame_sink_id_(frame_sink_id),
      manager_(manager),
      client_(client),
      holder_(client),
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
  if (manager_)
    manager_->Destroy(std::move(current_surface_));
  current_surface_.reset();
}

void SurfaceFactory::Reset() {
  EvictSurface();
  // Disown Surfaces that are still alive so that they don't try to unref
  // resources that we're not tracking any more.
  weak_factory_.InvalidateWeakPtrs();
  holder_.Reset();
}

void SurfaceFactory::SubmitCompositorFrame(const LocalFrameId& local_frame_id,
                                           CompositorFrame frame,
                                           const DrawCallback& callback) {
  TRACE_EVENT0("cc", "SurfaceFactory::SubmitCompositorFrame");
  DCHECK(local_frame_id.is_valid());
  std::unique_ptr<Surface> surface;
  bool create_new_surface =
      (!current_surface_ ||
       local_frame_id != current_surface_->surface_id().local_frame_id());
  if (!create_new_surface) {
    surface = std::move(current_surface_);
  } else {
    surface = Create(local_frame_id);
    gfx::Size frame_size;
    // CompositorFrames may not be populated with a RenderPass in unit tests.
    if (!frame.render_pass_list.empty())
      frame_size = frame.render_pass_list[0]->output_rect.size();
    manager_->SurfaceCreated(surface->surface_id(), frame_size,
                             frame.metadata.device_scale_factor);
  }
  surface->QueueFrame(std::move(frame), callback);
  if (!manager_->SurfaceModified(SurfaceId(frame_sink_id_, local_frame_id))) {
    TRACE_EVENT_INSTANT0("cc", "Damage not visible.", TRACE_EVENT_SCOPE_THREAD);
    surface->RunDrawCallbacks();
  }
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

void SurfaceFactory::ClearSurface() {
  if (!current_surface_)
    return;
  current_surface_->EvictFrame();
  manager_->SurfaceModified(current_surface_->surface_id());
}

void SurfaceFactory::WillDrawSurface(const LocalFrameId& id,
                                     const gfx::Rect& damage_rect) {
  client_->WillDrawSurface(id, damage_rect);
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

std::unique_ptr<Surface> SurfaceFactory::Create(
    const LocalFrameId& local_frame_id) {
  auto surface = base::MakeUnique<Surface>(
      SurfaceId(frame_sink_id_, local_frame_id), weak_factory_.GetWeakPtr());
  manager_->RegisterSurface(surface.get());
  return surface;
}

void SurfaceFactory::Destroy(std::unique_ptr<Surface> surface) {
  if (manager_)
    manager_->Destroy(std::move(surface));
}

}  // namespace cc
