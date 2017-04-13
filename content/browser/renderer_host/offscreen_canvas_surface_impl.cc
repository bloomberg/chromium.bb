// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/offscreen_canvas_surface_impl.h"

#include <utility>

#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "cc/surfaces/surface_manager.h"
#include "content/browser/compositor/frame_sink_manager_host.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/renderer_host/offscreen_canvas_compositor_frame_sink_manager.h"

namespace content {

OffscreenCanvasSurfaceImpl::OffscreenCanvasSurfaceImpl(
    const cc::FrameSinkId& parent_frame_sink_id,
    const cc::FrameSinkId& frame_sink_id,
    cc::mojom::FrameSinkManagerClientPtr client)
    : client_(std::move(client)),
      frame_sink_id_(frame_sink_id),
      parent_frame_sink_id_(parent_frame_sink_id) {
  OffscreenCanvasCompositorFrameSinkManager::GetInstance()
      ->RegisterOffscreenCanvasSurfaceInstance(frame_sink_id_, this);
}

OffscreenCanvasSurfaceImpl::~OffscreenCanvasSurfaceImpl() {
  if (has_created_compositor_frame_sink_) {
    GetFrameSinkManagerHost()->UnregisterFrameSinkHierarchy(
        parent_frame_sink_id_, frame_sink_id_);
  }
  OffscreenCanvasCompositorFrameSinkManager::GetInstance()
      ->UnregisterOffscreenCanvasSurfaceInstance(frame_sink_id_);
}

// static
void OffscreenCanvasSurfaceImpl::Create(
    const cc::FrameSinkId& parent_frame_sink_id,
    const cc::FrameSinkId& frame_sink_id,
    cc::mojom::FrameSinkManagerClientPtr client,
    blink::mojom::OffscreenCanvasSurfaceRequest request) {
  std::unique_ptr<OffscreenCanvasSurfaceImpl> impl =
      base::MakeUnique<OffscreenCanvasSurfaceImpl>(
          parent_frame_sink_id, frame_sink_id, std::move(client));
  OffscreenCanvasSurfaceImpl* surface_service = impl.get();
  surface_service->binding_ =
      mojo::MakeStrongBinding(std::move(impl), std::move(request));
}

void OffscreenCanvasSurfaceImpl::CreateCompositorFrameSink(
    cc::mojom::MojoCompositorFrameSinkClientPtr client,
    cc::mojom::MojoCompositorFrameSinkRequest request) {
  if (has_created_compositor_frame_sink_) {
    DLOG(ERROR) << "CreateCompositorFrameSink() called more than once.";
    return;
  }

  GetFrameSinkManagerHost()->CreateCompositorFrameSink(
      frame_sink_id_, std::move(request),
      mojo::MakeRequest(&compositor_frame_sink_private_), std::move(client));

  GetFrameSinkManagerHost()->RegisterFrameSinkHierarchy(parent_frame_sink_id_,
                                                        frame_sink_id_);
  has_created_compositor_frame_sink_ = true;
}

void OffscreenCanvasSurfaceImpl::OnSurfaceCreated(
    const cc::SurfaceInfo& surface_info) {
  DCHECK_EQ(surface_info.id().frame_sink_id(), frame_sink_id_);
  if (!current_local_surface_id_.is_valid() ||
      surface_info.id().local_surface_id() != current_local_surface_id_) {
    current_local_surface_id_ = surface_info.id().local_surface_id();
    if (client_)
      client_->OnSurfaceCreated(surface_info);
  }
}

void OffscreenCanvasSurfaceImpl::Require(const cc::SurfaceId& surface_id,
                                         const cc::SurfaceSequence& sequence) {
  GetSurfaceManager()->RequireSequence(surface_id, sequence);
}

void OffscreenCanvasSurfaceImpl::Satisfy(const cc::SurfaceSequence& sequence) {
  GetSurfaceManager()->SatisfySequence(sequence);
}

}  // namespace content
