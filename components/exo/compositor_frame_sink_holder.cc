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
    std::unique_ptr<CompositorFrameSink> frame_sink,
    cc::mojom::MojoCompositorFrameSinkClientRequest request)
    : surface_(surface),
      frame_sink_(std::move(frame_sink)),
      begin_frame_source_(base::MakeUnique<cc::ExternalBeginFrameSource>(this)),
      binding_(this, std::move(request)),
      weak_factory_(this) {
  surface_->AddSurfaceObserver(this);
}

bool CompositorFrameSinkHolder::HasReleaseCallbackForResource(
    cc::ResourceId id) {
  return release_callbacks_.find(id) != release_callbacks_.end();
}

void CompositorFrameSinkHolder::AddResourceReleaseCallback(
    cc::ResourceId id,
    std::unique_ptr<cc::SingleReleaseCallback> callback) {
  release_callbacks_[id] = std::make_pair(this, std::move(callback));
}

void CompositorFrameSinkHolder::ActivateFrameCallbacks(
    std::list<FrameCallback>* frame_callbacks) {
  active_frame_callbacks_.splice(active_frame_callbacks_.end(),
                                 *frame_callbacks);
  UpdateNeedsBeginFrame();
}

void CompositorFrameSinkHolder::CancelFrameCallbacks() {
  // Call pending frame callbacks with a null frame time to indicate that they
  // have been cancelled.
  for (const auto& frame_callback : active_frame_callbacks_)
    frame_callback.Run(base::TimeTicks());
}

void CompositorFrameSinkHolder::SetNeedsBeginFrame(bool needs_begin_frame) {
  needs_begin_frame_ = needs_begin_frame;
  OnNeedsBeginFrames(needs_begin_frame);
}

void CompositorFrameSinkHolder::Satisfy(const cc::SurfaceSequence& sequence) {
  frame_sink_->Satisfy(sequence);
}

void CompositorFrameSinkHolder::Require(const cc::SurfaceId& id,
                                        const cc::SurfaceSequence& sequence) {
  frame_sink_->Require(id.local_frame_id(), sequence);
}

////////////////////////////////////////////////////////////////////////////////
// cc::mojom::MojoCompositorFrameSinkClient overrides:

void CompositorFrameSinkHolder::DidReceiveCompositorFrameAck() {
  // TODO(staraz): Implement this
}

void CompositorFrameSinkHolder::OnBeginFrame(const cc::BeginFrameArgs& args) {
  while (!active_frame_callbacks_.empty()) {
    active_frame_callbacks_.front().Run(args.frame_time);
    active_frame_callbacks_.pop_front();
  }
  begin_frame_source_->OnBeginFrame(args);
}

void CompositorFrameSinkHolder::ReclaimResources(
    const cc::ReturnedResourceArray& resources) {
  for (auto& resource : resources) {
    auto it = release_callbacks_.find(resource.id);
    DCHECK(it != release_callbacks_.end());
    std::unique_ptr<cc::SingleReleaseCallback> callback =
        std::move(it->second.second);
    release_callbacks_.erase(it);
    callback->Run(resource.sync_token, resource.lost);
  }
}

void CompositorFrameSinkHolder::WillDrawSurface() {
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

////////////////////////////////////////////////////////////////////////////////
// SurfaceObserver overrides:

void CompositorFrameSinkHolder::OnSurfaceDestroying(Surface* surface) {
  surface_->RemoveSurfaceObserver(this);
  surface_ = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// ExoComopositorFrameSink, private:

CompositorFrameSinkHolder::~CompositorFrameSinkHolder() {}

void CompositorFrameSinkHolder::UpdateNeedsBeginFrame() {
  if (!begin_frame_source_)
    return;

  bool needs_begin_frame = !active_frame_callbacks_.empty();
  if (needs_begin_frame == needs_begin_frame_)
    return;

  needs_begin_frame_ = needs_begin_frame;
  OnNeedsBeginFrames(needs_begin_frame_);
}

}  // namespace exo
