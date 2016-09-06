// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/offscreen_canvas_frame_receiver_impl.h"

#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_manager.h"
#include "content/browser/compositor/surface_utils.h"

namespace content {

// static
void OffscreenCanvasFrameReceiverImpl::Create(
    mojo::InterfaceRequest<blink::mojom::OffscreenCanvasFrameReceiver>
        request) {
  // |binding_| will take ownership of OffscreenCanvasFrameReceiverImpl
  new OffscreenCanvasFrameReceiverImpl(std::move(request));
}

OffscreenCanvasFrameReceiverImpl::OffscreenCanvasFrameReceiverImpl(
    mojo::InterfaceRequest<blink::mojom::OffscreenCanvasFrameReceiver> request)
    : binding_(this, std::move(request)) {}

OffscreenCanvasFrameReceiverImpl::~OffscreenCanvasFrameReceiverImpl() {}

void OffscreenCanvasFrameReceiverImpl::SubmitCompositorFrame(
    const cc::SurfaceId& surface_id,
    cc::CompositorFrame frame) {
    cc::Surface* surface = GetSurfaceManager()->GetSurfaceForId(surface_id);
    if (surface) {
        surface->QueueFrame(std::move(frame), base::Closure());
    }
    // If surface doet not exist, drop the frame.
}

}  // namespace content
