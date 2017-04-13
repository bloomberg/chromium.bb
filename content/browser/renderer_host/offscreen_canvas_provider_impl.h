// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_PROVIDER_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_PROVIDER_IMPL_H_

#include "cc/surfaces/frame_sink_id.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "third_party/WebKit/public/platform/modules/offscreencanvas/offscreen_canvas_surface.mojom.h"

namespace content {

// Creates OffscreenCanvasSurfaces and MojoCompositorFrameSinks for a renderer.
class OffscreenCanvasProviderImpl
    : public blink::mojom::OffscreenCanvasProvider {
 public:
  OffscreenCanvasProviderImpl();
  ~OffscreenCanvasProviderImpl() override;

  void Add(blink::mojom::OffscreenCanvasProviderRequest request);

  // blink::mojom::OffscreenCanvasProvider implementation.
  void CreateOffscreenCanvasSurface(
      const cc::FrameSinkId& parent_frame_sink_id,
      const cc::FrameSinkId& frame_sink_id,
      cc::mojom::FrameSinkManagerClientPtr client,
      blink::mojom::OffscreenCanvasSurfaceRequest request) override;
  void CreateCompositorFrameSink(
      const cc::FrameSinkId& frame_sink_id,
      cc::mojom::MojoCompositorFrameSinkClientPtr client,
      cc::mojom::MojoCompositorFrameSinkRequest request) override;

 private:
  mojo::BindingSet<blink::mojom::OffscreenCanvasProvider> bindings_;

  DISALLOW_COPY_AND_ASSIGN(OffscreenCanvasProviderImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_PROVIDER_IMPL_H_
