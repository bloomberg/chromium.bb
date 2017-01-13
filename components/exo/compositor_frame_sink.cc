// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/compositor_frame_sink.h"

#include "base/memory/ptr_util.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_manager.h"
#include "components/exo/compositor_frame_sink_holder.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace exo {

////////////////////////////////////////////////////////////////////////////////
// ExoComopositorFrameSink, public:

CompositorFrameSink::CompositorFrameSink(const cc::FrameSinkId& frame_sink_id,
                                         cc::SurfaceManager* surface_manager,
                                         CompositorFrameSinkHolder* client)
    : support_(this, surface_manager, frame_sink_id, nullptr, nullptr),
      client_(client) {}

CompositorFrameSink::~CompositorFrameSink() {}

////////////////////////////////////////////////////////////////////////////////
// cc::mojom::MojoCompositorFrameSink overrides:

void CompositorFrameSink::SetNeedsBeginFrame(bool needs_begin_frame) {
  support_.SetNeedsBeginFrame(needs_begin_frame);
}

void CompositorFrameSink::SubmitCompositorFrame(
    const cc::LocalFrameId& local_frame_id,
    cc::CompositorFrame frame) {
  support_.SubmitCompositorFrame(local_frame_id, std::move(frame));
}

void CompositorFrameSink::AddSurfaceReferences(
    const std::vector<cc::SurfaceReference>& references) {
  // TODO(fsamuel, staraz): Implement this.
  NOTIMPLEMENTED();
}

void CompositorFrameSink::RemoveSurfaceReferences(
    const std::vector<cc::SurfaceReference>& references) {
  // TODO(fsamuel, staraz): Implement this.
  NOTIMPLEMENTED();
}

void CompositorFrameSink::EvictFrame() {
  support_.EvictFrame();
}

void CompositorFrameSink::Require(const cc::LocalFrameId& local_frame_id,
                                  const cc::SurfaceSequence& sequence) {
  support_.Require(local_frame_id, sequence);
}

void CompositorFrameSink::Satisfy(const cc::SurfaceSequence& sequence) {
  support_.Satisfy(sequence);
}

////////////////////////////////////////////////////////////////////////////////
// cc::CompositorFrameSinkSupportClient overrides:

void CompositorFrameSink::DidReceiveCompositorFrameAck() {
  client_->DidReceiveCompositorFrameAck();
}

void CompositorFrameSink::OnBeginFrame(const cc::BeginFrameArgs& args) {
  client_->OnBeginFrame(args);
}

void CompositorFrameSink::ReclaimResources(
    const cc::ReturnedResourceArray& resources) {
  client_->ReclaimResources(resources);
}

void CompositorFrameSink::WillDrawSurface() {
  client_->WillDrawSurface();
}

}  // namespace exo
