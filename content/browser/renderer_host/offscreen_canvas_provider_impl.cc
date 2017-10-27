// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/offscreen_canvas_provider_impl.h"

#include "base/bind.h"
#include "content/browser/renderer_host/offscreen_canvas_surface_impl.h"

namespace content {

OffscreenCanvasProviderImpl::OffscreenCanvasProviderImpl(
    viz::HostFrameSinkManager* host_frame_sink_manager,
    uint32_t renderer_client_id)
    : host_frame_sink_manager_(host_frame_sink_manager),
      renderer_client_id_(renderer_client_id) {}

OffscreenCanvasProviderImpl::~OffscreenCanvasProviderImpl() = default;

void OffscreenCanvasProviderImpl::Add(
    blink::mojom::OffscreenCanvasProviderRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void OffscreenCanvasProviderImpl::CreateOffscreenCanvasSurface(
    const viz::FrameSinkId& parent_frame_sink_id,
    const viz::FrameSinkId& frame_sink_id,
    blink::mojom::OffscreenCanvasSurfaceClientPtr client,
    blink::mojom::OffscreenCanvasSurfaceRequest request) {
  // TODO(kylechar): Kill the renderer too.
  if (parent_frame_sink_id.client_id() != renderer_client_id_) {
    DLOG(ERROR) << "Invalid parent client id " << parent_frame_sink_id;
    return;
  }
  if (frame_sink_id.client_id() != renderer_client_id_) {
    DLOG(ERROR) << "Invalid client id " << frame_sink_id;
    return;
  }

  auto destroy_callback = base::BindOnce(
      &OffscreenCanvasProviderImpl::DestroyOffscreenCanvasSurface,
      base::Unretained(this), frame_sink_id);

  canvas_map_[frame_sink_id] = std::make_unique<OffscreenCanvasSurfaceImpl>(
      host_frame_sink_manager_, parent_frame_sink_id, frame_sink_id,
      std::move(client), std::move(request), std::move(destroy_callback));
}

void OffscreenCanvasProviderImpl::CreateCompositorFrameSink(
    const viz::FrameSinkId& frame_sink_id,
    viz::mojom::CompositorFrameSinkClientPtr client,
    viz::mojom::CompositorFrameSinkRequest request) {
  // TODO(kylechar): Kill the renderer too.
  if (frame_sink_id.client_id() != renderer_client_id_) {
    DLOG(ERROR) << "Invalid client id " << frame_sink_id;
    return;
  }

  auto iter = canvas_map_.find(frame_sink_id);
  if (iter == canvas_map_.end()) {
    DLOG(ERROR) << "No OffscreenCanvasSurfaceImpl for " << frame_sink_id;
    return;
  }

  iter->second->CreateCompositorFrameSink(std::move(client),
                                          std::move(request));
}

void OffscreenCanvasProviderImpl::DestroyOffscreenCanvasSurface(
    viz::FrameSinkId frame_sink_id) {
  canvas_map_.erase(frame_sink_id);
}

}  // namespace content
