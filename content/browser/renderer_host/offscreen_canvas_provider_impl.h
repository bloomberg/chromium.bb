// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_PROVIDER_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_PROVIDER_IMPL_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "cc/surfaces/frame_sink_id.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "third_party/WebKit/public/platform/modules/offscreencanvas/offscreen_canvas_surface.mojom.h"

namespace content {

class OffscreenCanvasSurfaceImpl;

// Creates OffscreenCanvasSurfaces and MojoCompositorFrameSinks for a renderer.
class CONTENT_EXPORT OffscreenCanvasProviderImpl
    : public blink::mojom::OffscreenCanvasProvider {
 public:
  explicit OffscreenCanvasProviderImpl(uint32_t renderer_client_id);
  ~OffscreenCanvasProviderImpl() override;

  void Add(blink::mojom::OffscreenCanvasProviderRequest request);

  // blink::mojom::OffscreenCanvasProvider implementation.
  void CreateOffscreenCanvasSurface(
      const cc::FrameSinkId& parent_frame_sink_id,
      const cc::FrameSinkId& frame_sink_id,
      blink::mojom::OffscreenCanvasSurfaceClientPtr client,
      blink::mojom::OffscreenCanvasSurfaceRequest request) override;
  void CreateCompositorFrameSink(
      const cc::FrameSinkId& frame_sink_id,
      cc::mojom::MojoCompositorFrameSinkClientPtr client,
      cc::mojom::MojoCompositorFrameSinkRequest request) override;

 private:
  friend class OffscreenCanvasProviderImplTest;

  // Destroys the |canvas_map_| entry for |frame_sink_id|. Provided as a
  // callback to each OffscreenCanvasSurfaceImpl so they can destroy themselves.
  void DestroyOffscreenCanvasSurface(cc::FrameSinkId frame_sink_id);

  // FrameSinkIds for offscreen canvas must use the renderer client id.
  const uint32_t renderer_client_id_;

  mojo::BindingSet<blink::mojom::OffscreenCanvasProvider> bindings_;

  base::flat_map<cc::FrameSinkId, std::unique_ptr<OffscreenCanvasSurfaceImpl>>
      canvas_map_;

  DISALLOW_COPY_AND_ASSIGN(OffscreenCanvasProviderImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_PROVIDER_IMPL_H_
