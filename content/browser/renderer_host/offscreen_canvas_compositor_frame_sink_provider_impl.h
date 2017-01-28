// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_COMPOSITOR_FRAME_SINK_PROVIDER_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_COMPOSITOR_FRAME_SINK_PROVIDER_IMPL_H_

#include <unordered_map>

#include "content/browser/renderer_host/offscreen_canvas_compositor_frame_sink.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "third_party/WebKit/public/platform/modules/offscreencanvas/offscreen_canvas_surface.mojom.h"

namespace content {

// TODO(fsamuel): This should be replaced with the DisplayCompositor interface.
class OffscreenCanvasCompositorFrameSinkProviderImpl
    : public blink::mojom::OffscreenCanvasCompositorFrameSinkProvider {
 public:
  OffscreenCanvasCompositorFrameSinkProviderImpl();
  ~OffscreenCanvasCompositorFrameSinkProviderImpl() override;

  void Add(
      blink::mojom::OffscreenCanvasCompositorFrameSinkProviderRequest request);

  // blink::mojom::OffscreenCanvasCompositorFrameSinkProvider implementation.
  void CreateCompositorFrameSink(
      const cc::FrameSinkId& frame_sink_id,
      cc::mojom::MojoCompositorFrameSinkClientPtr client,
      cc::mojom::MojoCompositorFrameSinkRequest request) override;

  cc::SurfaceManager* GetSurfaceManager();

  void OnCompositorFrameSinkClientConnectionLost(
      const cc::FrameSinkId& frame_sink_id);
  void OnCompositorFrameSinkClientDestroyed(
      const cc::FrameSinkId& frame_sink_id);

 private:
  std::unordered_map<cc::FrameSinkId,
                     std::unique_ptr<OffscreenCanvasCompositorFrameSink>,
                     cc::FrameSinkIdHash>
      compositor_frame_sinks_;

  mojo::BindingSet<blink::mojom::OffscreenCanvasCompositorFrameSinkProvider>
      bindings_;

  DISALLOW_COPY_AND_ASSIGN(OffscreenCanvasCompositorFrameSinkProviderImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_COMPOSITOR_FRAME_SINK_PROVIDER_IMPL_H_
