// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_PROVIDER_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_PROVIDER_IMPL_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "third_party/WebKit/public/platform/modules/offscreencanvas/offscreen_canvas_surface.mojom.h"

namespace viz {
class HostFrameSinkManager;
}

namespace content {

class OffscreenCanvasSurfaceImpl;

// Creates OffscreenCanvasSurfaces and CompositorFrameSinks for a renderer.
class CONTENT_EXPORT OffscreenCanvasProviderImpl
    : public blink::mojom::OffscreenCanvasProvider {
 public:
  OffscreenCanvasProviderImpl(
      viz::HostFrameSinkManager* host_frame_sink_manager,
      uint32_t renderer_client_id);
  ~OffscreenCanvasProviderImpl() override;

  void Add(blink::mojom::OffscreenCanvasProviderRequest request);

  // blink::mojom::OffscreenCanvasProvider implementation.
  void CreateOffscreenCanvasSurface(
      const viz::FrameSinkId& parent_frame_sink_id,
      const viz::FrameSinkId& frame_sink_id,
      blink::mojom::OffscreenCanvasSurfaceClientPtr client,
      blink::mojom::OffscreenCanvasSurfaceRequest request) override;
  void CreateCompositorFrameSink(
      const viz::FrameSinkId& frame_sink_id,
      viz::mojom::CompositorFrameSinkClientPtr client,
      viz::mojom::CompositorFrameSinkRequest request) override;

 private:
  friend class OffscreenCanvasProviderImplTest;

  // Destroys the |canvas_map_| entry for |frame_sink_id|. Provided as a
  // callback to each OffscreenCanvasSurfaceImpl so they can destroy themselves.
  void DestroyOffscreenCanvasSurface(viz::FrameSinkId frame_sink_id);

  viz::HostFrameSinkManager* const host_frame_sink_manager_;

  // FrameSinkIds for offscreen canvas must use the renderer client id.
  const uint32_t renderer_client_id_;

  mojo::BindingSet<blink::mojom::OffscreenCanvasProvider> bindings_;

  base::flat_map<viz::FrameSinkId, std::unique_ptr<OffscreenCanvasSurfaceImpl>>
      canvas_map_;

  DISALLOW_COPY_AND_ASSIGN(OffscreenCanvasProviderImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_PROVIDER_IMPL_H_
