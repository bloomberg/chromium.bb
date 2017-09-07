// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/offscreen_canvas_surface_impl.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "content/browser/compositor/surface_utils.h"

namespace content {

OffscreenCanvasSurfaceImpl::OffscreenCanvasSurfaceImpl(
    viz::HostFrameSinkManager* host_frame_sink_manager,
    const viz::FrameSinkId& parent_frame_sink_id,
    const viz::FrameSinkId& frame_sink_id,
    blink::mojom::OffscreenCanvasSurfaceClientPtr client,
    blink::mojom::OffscreenCanvasSurfaceRequest request,
    DestroyCallback destroy_callback)
    : host_frame_sink_manager_(host_frame_sink_manager),
      client_(std::move(client)),
      binding_(this, std::move(request)),
      destroy_callback_(std::move(destroy_callback)),
      frame_sink_id_(frame_sink_id),
      parent_frame_sink_id_(parent_frame_sink_id) {
  binding_.set_connection_error_handler(
      base::BindOnce(&OffscreenCanvasSurfaceImpl::OnSurfaceConnectionClosed,
                     base::Unretained(this)));
  host_frame_sink_manager_->RegisterFrameSinkId(frame_sink_id_, this);
#if DCHECK_IS_ON()
  host_frame_sink_manager_->SetFrameSinkDebugLabel(
      frame_sink_id_, "OffscreenCanvasSurfaceImpl");
#endif
}

OffscreenCanvasSurfaceImpl::~OffscreenCanvasSurfaceImpl() {
  if (has_created_compositor_frame_sink_) {
    host_frame_sink_manager_->UnregisterFrameSinkHierarchy(
        parent_frame_sink_id_, frame_sink_id_);
    host_frame_sink_manager_->InvalidateFrameSinkId(frame_sink_id_);
  }
}

void OffscreenCanvasSurfaceImpl::CreateCompositorFrameSink(
    viz::mojom::CompositorFrameSinkClientPtr client,
    viz::mojom::CompositorFrameSinkRequest request) {
  if (has_created_compositor_frame_sink_) {
    DLOG(ERROR) << "CreateCompositorFrameSink() called more than once.";
    return;
  }

  host_frame_sink_manager_->CreateCompositorFrameSink(
      frame_sink_id_, std::move(request), std::move(client));

  host_frame_sink_manager_->RegisterFrameSinkHierarchy(parent_frame_sink_id_,
                                                       frame_sink_id_);
  has_created_compositor_frame_sink_ = true;
}

void OffscreenCanvasSurfaceImpl::OnFirstSurfaceActivation(
    const viz::SurfaceInfo& surface_info) {
  DCHECK_EQ(surface_info.id().frame_sink_id(), frame_sink_id_);

  local_surface_id_ = surface_info.id().local_surface_id();
  if (client_)
    client_->OnFirstSurfaceActivation(surface_info);
}

void OffscreenCanvasSurfaceImpl::Require(const viz::SurfaceId& surface_id,
                                         const viz::SurfaceSequence& sequence) {
  auto* surface_manager = GetFrameSinkManager()->surface_manager();
  if (!surface_manager->using_surface_references())
    surface_manager->RequireSequence(surface_id, sequence);
}

void OffscreenCanvasSurfaceImpl::Satisfy(const viz::SurfaceSequence& sequence) {
  auto* surface_manager = GetFrameSinkManager()->surface_manager();
  if (!surface_manager->using_surface_references())
    surface_manager->SatisfySequence(sequence);
}

void OffscreenCanvasSurfaceImpl::OnSurfaceConnectionClosed() {
  std::move(destroy_callback_).Run();
}

}  // namespace content
