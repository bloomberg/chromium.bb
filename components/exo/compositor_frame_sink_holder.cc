// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/compositor_frame_sink_holder.h"

#include "cc/output/compositor_frame_sink.h"
#include "cc/resources/returned_resource.h"
#include "components/exo/surface.h"

namespace exo {

////////////////////////////////////////////////////////////////////////////////
// CompositorFrameSinkHolder, public:

CompositorFrameSinkHolder::CompositorFrameSinkHolder(
    Surface* surface,
    std::unique_ptr<cc::CompositorFrameSink> frame_sink)
    : surface_(surface),
      frame_sink_(std::move(frame_sink)),
      weak_factory_(this) {
  surface_->AddSurfaceObserver(this);
  frame_sink_->BindToClient(this);
}

CompositorFrameSinkHolder::~CompositorFrameSinkHolder() {
  frame_sink_->DetachFromClient();
  if (surface_)
    surface_->RemoveSurfaceObserver(this);

  // Release all resources which aren't returned from CompositorFrameSink.
  for (auto& callback : release_callbacks_)
    callback.second.Run(gpu::SyncToken(), false);
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
// cc::CompositorFrameSinkClient overrides:

void CompositorFrameSinkHolder::SetBeginFrameSource(
    cc::BeginFrameSource* source) {
  if (surface_)
    surface_->SetBeginFrameSource(source);
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

void CompositorFrameSinkHolder::DidReceiveCompositorFrameAck() {
  if (surface_)
    surface_->DidReceiveCompositorFrameAck();
}

////////////////////////////////////////////////////////////////////////////////
// SurfaceObserver overrides:

void CompositorFrameSinkHolder::OnSurfaceDestroying(Surface* surface) {
  surface_->RemoveSurfaceObserver(this);
  surface_ = nullptr;
}

}  // namespace exo
