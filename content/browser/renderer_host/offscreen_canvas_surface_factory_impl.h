// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_SURFACE_FACTORY_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_SURFACE_FACTORY_IMPL_H_

#include "third_party/WebKit/public/platform/modules/offscreencanvas/offscreen_canvas_surface.mojom.h"

namespace content {

class OffscreenCanvasSurfaceFactoryImpl
    : public blink::mojom::OffscreenCanvasSurfaceFactory {
 public:
  OffscreenCanvasSurfaceFactoryImpl() {}
  ~OffscreenCanvasSurfaceFactoryImpl() override {}

  static void Create(
      blink::mojom::OffscreenCanvasSurfaceFactoryRequest request);

  // blink::mojom::OffscreenCanvasSurfaceFactory implementation.
  void CreateOffscreenCanvasSurface(
      const cc::FrameSinkId& frame_sink_id,
      blink::mojom::OffscreenCanvasSurfaceClientPtr client,
      blink::mojom::OffscreenCanvasSurfaceRequest request) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(OffscreenCanvasSurfaceFactoryImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_SURFACE_FACTORY_IMPL_H_
