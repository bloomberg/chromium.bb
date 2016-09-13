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

OffscreenCanvasFrameReceiverImpl::~OffscreenCanvasFrameReceiverImpl() {
  if (surface_factory_) {
    if (!GetSurfaceManager()) {
      // Inform SurfaceFactory that SurfaceManager's no longer alive to
      // avoid its destruction error.
      surface_factory_->DidDestroySurfaceManager();
    } else {
      GetSurfaceManager()->InvalidateSurfaceClientId(surface_id_.client_id());
    }
    surface_factory_->Destroy(surface_id_);
  }
}

// static
void OffscreenCanvasFrameReceiverImpl::Create(
    blink::mojom::OffscreenCanvasFrameReceiverRequest request) {
  mojo::MakeStrongBinding(base::MakeUnique<OffscreenCanvasFrameReceiverImpl>(),
                          std::move(request));
}

void OffscreenCanvasFrameReceiverImpl::SubmitCompositorFrame(
    const cc::SurfaceId& surface_id,
    cc::CompositorFrame frame) {
  if (!surface_factory_) {
    cc::SurfaceManager* manager = GetSurfaceManager();
    surface_factory_ = base::MakeUnique<cc::SurfaceFactory>(manager, this);
    surface_factory_->Create(surface_id);

    GetSurfaceManager()->RegisterSurfaceClientId(surface_id.client_id());
  }
  if (surface_id_.is_null()) {
    surface_id_ = surface_id;
    }
    surface_factory_->SubmitCompositorFrame(surface_id, std::move(frame),
                                            base::Closure());
}

// TODO(619136): Implement cc::SurfaceFactoryClient functions for resources
// return.
void OffscreenCanvasFrameReceiverImpl::ReturnResources(
    const cc::ReturnedResourceArray& resources) {}

void OffscreenCanvasFrameReceiverImpl::WillDrawSurface(
    const cc::SurfaceId& id,
    const gfx::Rect& damage_rect) {}

void OffscreenCanvasFrameReceiverImpl::SetBeginFrameSource(
    cc::BeginFrameSource* begin_frame_source) {}

}  // namespace content
