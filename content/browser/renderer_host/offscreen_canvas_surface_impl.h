// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_SURFACE_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_SURFACE_IMPL_H_

#include "cc/ipc/display_compositor.mojom.h"
#include "cc/surfaces/surface_id.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "third_party/WebKit/public/platform/modules/offscreencanvas/offscreen_canvas_surface.mojom.h"

namespace content {

class CONTENT_EXPORT OffscreenCanvasSurfaceImpl
    : public blink::mojom::OffscreenCanvasSurface {
 public:
  OffscreenCanvasSurfaceImpl(const cc::FrameSinkId& parent_frame_sink_id,
                             const cc::FrameSinkId& frame_sink_id,
                             cc::mojom::DisplayCompositorClientPtr client);
  ~OffscreenCanvasSurfaceImpl() override;

  static void Create(const cc::FrameSinkId& parent_frame_sink_id,
                     const cc::FrameSinkId& frame_sink_id,
                     cc::mojom::DisplayCompositorClientPtr client,
                     blink::mojom::OffscreenCanvasSurfaceRequest request);

  void OnSurfaceCreated(const cc::SurfaceInfo& surface_info);

  // blink::mojom::OffscreenCanvasSurface implementation.
  void Require(const cc::SurfaceId& surface_id,
               const cc::SurfaceSequence& sequence) override;
  void Satisfy(const cc::SurfaceSequence& sequence) override;

  const cc::FrameSinkId& frame_sink_id() const { return frame_sink_id_; }

  const cc::FrameSinkId& parent_frame_sink_id() const {
    return parent_frame_sink_id_;
  }

  const cc::LocalSurfaceId& current_local_surface_id() const {
    return current_local_surface_id_;
  }

 private:
  cc::mojom::DisplayCompositorClientPtr client_;
  mojo::StrongBindingPtr<blink::mojom::OffscreenCanvasSurface> binding_;

  // Surface-related state
  const cc::FrameSinkId frame_sink_id_;
  cc::LocalSurfaceId current_local_surface_id_;
  const cc::FrameSinkId parent_frame_sink_id_;

  DISALLOW_COPY_AND_ASSIGN(OffscreenCanvasSurfaceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_SURFACE_IMPL_H_
