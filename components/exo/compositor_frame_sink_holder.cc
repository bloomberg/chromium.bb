// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/compositor_frame_sink_holder.h"

#include "cc/resources/returned_resource.h"
#include "components/exo/surface.h"

namespace exo {

////////////////////////////////////////////////////////////////////////////////
// CompositorFrameSinkHolder, public:

CompositorFrameSinkHolder::CompositorFrameSinkHolder(
    Surface* surface,
    const cc::FrameSinkId& frame_sink_id,
    cc::SurfaceManager* surface_manager)
    : surface_(surface),
      frame_sink_(
          new CompositorFrameSink(frame_sink_id, surface_manager, this)),
      begin_frame_source_(base::MakeUnique<cc::ExternalBeginFrameSource>(this)),
      weak_factory_(this) {
  surface_->AddSurfaceObserver(this);
  surface_->SetBeginFrameSource(begin_frame_source_.get());
}

bool CompositorFrameSinkHolder::HasReleaseCallbackForResource(
    cc::ResourceId id) {
  return release_callbacks_.find(id) != release_callbacks_.end();
}

void CompositorFrameSinkHolder::SetResourceReleaseCallback(
    cc::ResourceId id,
    const cc::ReleaseCallback& callback) {
  DCHECK(!callback.is_null());
  release_callbacks_[id] = callback;
}

////////////////////////////////////////////////////////////////////////////////
// cc::mojom::MojoCompositorFrameSinkClient overrides:

void CompositorFrameSinkHolder::DidReceiveCompositorFrameAck() {
  // TODO(staraz): Implement this
}

void CompositorFrameSinkHolder::OnBeginFrame(const cc::BeginFrameArgs& args) {
  begin_frame_source_->OnBeginFrame(args);
}

void CompositorFrameSinkHolder::ReclaimResources(
    const cc::ReturnedResourceArray& resources) {
  for (auto& resource : resources) {
    auto it = release_callbacks_.find(resource.id);
    DCHECK(it != release_callbacks_.end());
    if (it != release_callbacks_.end()) {
      it->second.Run(resource.sync_token, resource.lost);
      release_callbacks_.erase(it);
    }
  }
}

void CompositorFrameSinkHolder::WillDrawSurface(
    const cc::LocalSurfaceId& local_surface_id,
    const gfx::Rect& damage_rect) {
  if (surface_)
    surface_->WillDraw();
}

////////////////////////////////////////////////////////////////////////////////
// cc::ExternalBeginFrameSourceClient overrides:

void CompositorFrameSinkHolder::OnNeedsBeginFrames(bool needs_begin_frames) {
  frame_sink_->SetNeedsBeginFrame(needs_begin_frames);
}

void CompositorFrameSinkHolder::OnDidFinishFrame(const cc::BeginFrameAck& ack) {
  // If there was damage, the submitted CompositorFrame includes the ack.
  if (!ack.has_damage)
    frame_sink_->BeginFrameDidNotSwap(ack);
}

////////////////////////////////////////////////////////////////////////////////
// SurfaceObserver overrides:

void CompositorFrameSinkHolder::OnSurfaceDestroying(Surface* surface) {
  surface_->RemoveSurfaceObserver(this);
  surface_ = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// ExoComopositorFrameSink, private:

CompositorFrameSinkHolder::~CompositorFrameSinkHolder() {
  if (surface_)
    surface_->RemoveSurfaceObserver(this);
}

}  // namespace exo
