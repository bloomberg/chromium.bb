// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_FRAME_RECEIVER_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_FRAME_RECEIVER_IMPL_H_

#include "third_party/WebKit/public/platform/modules/offscreencanvas/offscreen_canvas_surface.mojom.h"

namespace content {

class OffscreenCanvasFrameReceiverImpl
    : public blink::mojom::OffscreenCanvasFrameReceiver {
 public:
  OffscreenCanvasFrameReceiverImpl();
  ~OffscreenCanvasFrameReceiverImpl() override;

  static void Create(blink::mojom::OffscreenCanvasFrameReceiverRequest request);

  void SubmitCompositorFrame(const cc::SurfaceId& surface_id,
                             cc::CompositorFrame frame) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(OffscreenCanvasFrameReceiverImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_FRAME_RECEIVER_IMPL_H_
