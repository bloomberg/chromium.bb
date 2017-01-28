// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/offscreen_canvas_compositor_frame_sink_provider_impl.h"

#include "base/memory/ptr_util.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/renderer_host/offscreen_canvas_compositor_frame_sink.h"
#include "content/browser/renderer_host/offscreen_canvas_surface_manager.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace content {

OffscreenCanvasCompositorFrameSinkProviderImpl::
    OffscreenCanvasCompositorFrameSinkProviderImpl() {}

OffscreenCanvasCompositorFrameSinkProviderImpl::
    ~OffscreenCanvasCompositorFrameSinkProviderImpl() {}

void OffscreenCanvasCompositorFrameSinkProviderImpl::Add(
    blink::mojom::OffscreenCanvasCompositorFrameSinkProviderRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void OffscreenCanvasCompositorFrameSinkProviderImpl::CreateCompositorFrameSink(
    const cc::FrameSinkId& frame_sink_id,
    cc::mojom::MojoCompositorFrameSinkClientPtr client,
    cc::mojom::MojoCompositorFrameSinkRequest request) {
  compositor_frame_sinks_[frame_sink_id] =
      base::MakeUnique<OffscreenCanvasCompositorFrameSink>(
          this, frame_sink_id, std::move(request), std::move(client));

  OffscreenCanvasSurfaceManager::GetInstance()->RegisterFrameSinkToParent(
      frame_sink_id);
}

cc::SurfaceManager*
OffscreenCanvasCompositorFrameSinkProviderImpl::GetSurfaceManager() {
  return content::GetSurfaceManager();
}

void OffscreenCanvasCompositorFrameSinkProviderImpl::
    OnCompositorFrameSinkClientConnectionLost(
        const cc::FrameSinkId& frame_sink_id) {
  // TODO(fsamuel, xlai): Investigate why this function is not fired when user
  // close down the window that has OffscreenCanvas commit().
  compositor_frame_sinks_.erase(frame_sink_id);
}

void OffscreenCanvasCompositorFrameSinkProviderImpl::
    OnCompositorFrameSinkClientDestroyed(const cc::FrameSinkId& frame_sink_id) {
  OffscreenCanvasSurfaceManager::GetInstance()->UnregisterFrameSinkFromParent(
      frame_sink_id);
}

}  // namespace content
