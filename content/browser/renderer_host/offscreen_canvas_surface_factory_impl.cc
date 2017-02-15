// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/offscreen_canvas_surface_factory_impl.h"

#include "base/memory/ptr_util.h"
#include "cc/surfaces/frame_sink_id.h"
#include "content/browser/renderer_host/offscreen_canvas_surface_impl.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace content {

// static
void OffscreenCanvasSurfaceFactoryImpl::Create(
    blink::mojom::OffscreenCanvasSurfaceFactoryRequest request) {
  mojo::MakeStrongBinding(base::MakeUnique<OffscreenCanvasSurfaceFactoryImpl>(),
                          std::move(request));
}

void OffscreenCanvasSurfaceFactoryImpl::CreateOffscreenCanvasSurface(
    const cc::FrameSinkId& parent_frame_sink_id,
    const cc::FrameSinkId& frame_sink_id,
    cc::mojom::DisplayCompositorClientPtr client,
    blink::mojom::OffscreenCanvasSurfaceRequest request) {
  OffscreenCanvasSurfaceImpl::Create(parent_frame_sink_id, frame_sink_id,
                                     std::move(client), std::move(request));
}

}  // namespace content
