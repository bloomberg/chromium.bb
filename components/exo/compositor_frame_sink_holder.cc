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

void CompositorFrameSinkHolder::SetNeedsBeginFrame(bool needs_begin_frame) {
  needs_begin_frame_ = needs_begin_frame;
  OnNeedsBeginFrames(needs_begin_frame);
}

////////////////////////////////////////////////////////////////////////////////
// cc::mojom::MojoCompositorFrameSinkClient overrides:

void CompositorFrameSinkHolder::DidReceiveCompositorFrameAck() {
  // TODO(staraz): Implement this
}

void CompositorFrameSinkHolder::OnBeginFrame(const cc::BeginFrameArgs& args) {
  // TODO(eseckler): Hook up |surface_| to the ExternalBeginFrameSource.
  if (surface_)
    surface_->BeginFrame(args.frame_time);

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

  UpdateNeedsBeginFrame();
}

////////////////////////////////////////////////////////////////////////////////
// cc::BeginFrameObserver overrides:

const cc::BeginFrameArgs& CompositorFrameSinkHolder::LastUsedBeginFrameArgs()
    const {
  return last_begin_frame_args_;
}

void CompositorFrameSinkHolder::OnBeginFrameSourcePausedChanged(bool paused) {}

////////////////////////////////////////////////////////////////////////////////
// cc::ExternalBeginFrameSouceClient overrides:

void CompositorFrameSinkHolder::OnNeedsBeginFrames(bool needs_begin_frames) {
  frame_sink_->SetNeedsBeginFrame(needs_begin_frames);
}

void CompositorFrameSinkHolder::OnDidFinishFrame(const cc::BeginFrameAck& ack) {
  // TODO(eseckler): Pass on the ack to frame_sink_.
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

void CompositorFrameSinkHolder::UpdateNeedsBeginFrame() {
  if (!begin_frame_source_)
    return;

  bool needs_begin_frame = surface_ && surface_->NeedsBeginFrame();
  if (needs_begin_frame == needs_begin_frame_)
    return;

  needs_begin_frame_ = needs_begin_frame;
  OnNeedsBeginFrames(needs_begin_frame_);
}

}  // namespace exo
