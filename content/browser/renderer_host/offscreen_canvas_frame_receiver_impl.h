// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_FRAME_RECEIVER_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_FRAME_RECEIVER_IMPL_H_

#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "third_party/WebKit/public/platform/modules/offscreencanvas/offscreen_canvas_surface.mojom.h"

namespace content {

class OffscreenCanvasFrameReceiverImpl
    : public blink::mojom::OffscreenCanvasFrameReceiver {
 public:
  static void Create(mojo::InterfaceRequest<
                     blink::mojom::OffscreenCanvasFrameReceiver> request);

  void SubmitCompositorFrame(const cc::SurfaceId& surface_id,
                             cc::CompositorFrame frame) override;

 private:
  ~OffscreenCanvasFrameReceiverImpl() override;
  explicit OffscreenCanvasFrameReceiverImpl(
      mojo::InterfaceRequest<blink::mojom::OffscreenCanvasFrameReceiver>
          request);

  mojo::StrongBinding<blink::mojom::OffscreenCanvasFrameReceiver> binding_;

  DISALLOW_COPY_AND_ASSIGN(OffscreenCanvasFrameReceiverImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_FRAME_RECEIVER_IMPL_H_
