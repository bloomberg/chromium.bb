// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/offscreen_canvas_provider_impl.h"

#include "content/browser/compositor/surface_utils.h"
#include "content/browser/renderer_host/offscreen_canvas_compositor_frame_sink_manager.h"
#include "content/browser/renderer_host/offscreen_canvas_surface_impl.h"

namespace content {

OffscreenCanvasProviderImpl::OffscreenCanvasProviderImpl() = default;

OffscreenCanvasProviderImpl::~OffscreenCanvasProviderImpl() = default;

void OffscreenCanvasProviderImpl::Add(
    blink::mojom::OffscreenCanvasProviderRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void OffscreenCanvasProviderImpl::CreateOffscreenCanvasSurface(
    const cc::FrameSinkId& parent_frame_sink_id,
    const cc::FrameSinkId& frame_sink_id,
    cc::mojom::FrameSinkManagerClientPtr client,
    blink::mojom::OffscreenCanvasSurfaceRequest request) {
  OffscreenCanvasSurfaceImpl::Create(parent_frame_sink_id, frame_sink_id,
                                     std::move(client), std::move(request));
}

void OffscreenCanvasProviderImpl::CreateCompositorFrameSink(
    const cc::FrameSinkId& frame_sink_id,
    cc::mojom::MojoCompositorFrameSinkClientPtr client,
    cc::mojom::MojoCompositorFrameSinkRequest request) {
  // TODO(kylechar): Add test for bad |frame_sink_id|.
  auto* manager = OffscreenCanvasCompositorFrameSinkManager::GetInstance();
  auto* surface_impl = manager->GetSurfaceInstance(frame_sink_id);
  if (!surface_impl) {
    DLOG(ERROR) << "No OffscreenCanvasSurfaceImpl for " << frame_sink_id;
    return;
  }

  surface_impl->CreateCompositorFrameSink(std::move(client),
                                          std::move(request));
}

}  // namespace content
