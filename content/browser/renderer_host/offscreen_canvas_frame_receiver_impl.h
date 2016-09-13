// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_FRAME_RECEIVER_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_FRAME_RECEIVER_IMPL_H_

#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_factory_client.h"
#include "third_party/WebKit/public/platform/modules/offscreencanvas/offscreen_canvas_surface.mojom.h"

namespace content {

class OffscreenCanvasFrameReceiverImpl
    : public blink::mojom::OffscreenCanvasFrameReceiver,
      public cc::SurfaceFactoryClient {
 public:
  OffscreenCanvasFrameReceiverImpl();
  ~OffscreenCanvasFrameReceiverImpl() override;

  static void Create(blink::mojom::OffscreenCanvasFrameReceiverRequest request);

  void SubmitCompositorFrame(const cc::SurfaceId& surface_id,
                             cc::CompositorFrame frame) override;

  // cc::SurfaceFactoryClient implementation.
  void ReturnResources(const cc::ReturnedResourceArray& resources) override;
  void WillDrawSurface(const cc::SurfaceId& id,
                       const gfx::Rect& damage_rect) override;
  void SetBeginFrameSource(cc::BeginFrameSource* begin_frame_source) override;

 private:
  cc::SurfaceId surface_id_;
  std::unique_ptr<cc::SurfaceFactory> surface_factory_;

  DISALLOW_COPY_AND_ASSIGN(OffscreenCanvasFrameReceiverImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_FRAME_RECEIVER_IMPL_H_
