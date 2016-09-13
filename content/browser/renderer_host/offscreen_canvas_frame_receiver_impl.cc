// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/offscreen_canvas_frame_receiver_impl.h"

#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_manager.h"
#include "content/browser/compositor/surface_utils.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace content {

OffscreenCanvasFrameReceiverImpl::OffscreenCanvasFrameReceiverImpl() {}

OffscreenCanvasFrameReceiverImpl::~OffscreenCanvasFrameReceiverImpl() {}

// static
void OffscreenCanvasFrameReceiverImpl::Create(
    blink::mojom::OffscreenCanvasFrameReceiverRequest request) {
  mojo::MakeStrongBinding(base::MakeUnique<OffscreenCanvasFrameReceiverImpl>(),
                          std::move(request));
}

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
