// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/offscreen_canvas_surface_impl.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "cc/surfaces/surface_manager.h"
#include "content/browser/compositor/frame_sink_manager_host.h"
#include "content/browser/compositor/surface_utils.h"

namespace content {

OffscreenCanvasSurfaceImpl::OffscreenCanvasSurfaceImpl(
    const cc::FrameSinkId& parent_frame_sink_id,
    const cc::FrameSinkId& frame_sink_id,
    blink::mojom::OffscreenCanvasSurfaceClientPtr client,
    blink::mojom::OffscreenCanvasSurfaceRequest request,
    DestroyCallback destroy_callback)
    : client_(std::move(client)),
      binding_(this, std::move(request)),
      destroy_callback_(std::move(destroy_callback)),
      frame_sink_id_(frame_sink_id),
      parent_frame_sink_id_(parent_frame_sink_id) {
  binding_.set_connection_error_handler(
      base::Bind(&OffscreenCanvasSurfaceImpl::OnSurfaceConnectionClosed,
                 base::Unretained(this)));
  GetFrameSinkManagerHost()->AddObserver(this);
}

OffscreenCanvasSurfaceImpl::~OffscreenCanvasSurfaceImpl() {
  if (has_created_compositor_frame_sink_) {
    GetFrameSinkManagerHost()->UnregisterFrameSinkHierarchy(
        parent_frame_sink_id_, frame_sink_id_);
  }
  GetFrameSinkManagerHost()->RemoveObserver(this);
}

void OffscreenCanvasSurfaceImpl::CreateCompositorFrameSink(
    cc::mojom::MojoCompositorFrameSinkClientPtr client,
    cc::mojom::MojoCompositorFrameSinkRequest request) {
  if (has_created_compositor_frame_sink_) {
    DLOG(ERROR) << "CreateCompositorFrameSink() called more than once.";
    return;
  }

  GetFrameSinkManagerHost()->CreateCompositorFrameSink(
      frame_sink_id_, std::move(request),
      mojo::MakeRequest(&compositor_frame_sink_private_), std::move(client));

  GetFrameSinkManagerHost()->RegisterFrameSinkHierarchy(parent_frame_sink_id_,
                                                        frame_sink_id_);
  has_created_compositor_frame_sink_ = true;
}

void OffscreenCanvasSurfaceImpl::OnSurfaceCreated(
    const cc::SurfaceInfo& surface_info) {
  if (surface_info.id().frame_sink_id() != frame_sink_id_)
    return;

  local_surface_id_ = surface_info.id().local_surface_id();
  if (client_)
    client_->OnSurfaceCreated(surface_info);
}

void OffscreenCanvasSurfaceImpl::Require(const cc::SurfaceId& surface_id,
                                         const cc::SurfaceSequence& sequence) {
  GetSurfaceManager()->RequireSequence(surface_id, sequence);
}

void OffscreenCanvasSurfaceImpl::Satisfy(const cc::SurfaceSequence& sequence) {
  GetSurfaceManager()->SatisfySequence(sequence);
}

void OffscreenCanvasSurfaceImpl::OnSurfaceConnectionClosed() {
  std::move(destroy_callback_).Run();
}

}  // namespace content
