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
    : support_(this,
               surface_manager,
               frame_sink_id,
               false /* is_root */,
               true /* handles_frame_sink_id_invalidation */,
               true /* needs_sync_points */),
      client_(client) {}

CompositorFrameSink::~CompositorFrameSink() {}

////////////////////////////////////////////////////////////////////////////////
// cc::mojom::MojoCompositorFrameSink overrides:

void CompositorFrameSink::SetNeedsBeginFrame(bool needs_begin_frame) {
  support_.SetNeedsBeginFrame(needs_begin_frame);
}

void CompositorFrameSink::SubmitCompositorFrame(
    const cc::LocalSurfaceId& local_surface_id,
    cc::CompositorFrame frame) {
  support_.SubmitCompositorFrame(local_surface_id, std::move(frame));
}

void CompositorFrameSink::EvictFrame() {
  support_.EvictFrame();
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

void CompositorFrameSink::WillDrawSurface(
    const cc::LocalSurfaceId& local_surface_id,
    const gfx::Rect& damage_rect) {
  client_->WillDrawSurface(local_surface_id, damage_rect);
}

}  // namespace exo
