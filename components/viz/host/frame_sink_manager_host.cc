// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/host/frame_sink_manager_host.h"

#include <utility>

#include "cc/surfaces/surface_info.h"
#include "cc/surfaces/surface_manager.h"

namespace viz {

FrameSinkManagerHost::FrameSinkManagerHost() : binding_(this) {}

FrameSinkManagerHost::~FrameSinkManagerHost() = default;

void FrameSinkManagerHost::BindManagerClientAndSetManagerPtr(
    cc::mojom::FrameSinkManagerClientRequest request,
    cc::mojom::FrameSinkManagerPtr ptr) {
  DCHECK(!binding_.is_bound());
  binding_.Bind(std::move(request));
  frame_sink_manager_ptr_ = std::move(ptr);
}

void FrameSinkManagerHost::AddObserver(FrameSinkObserver* observer) {
  observers_.AddObserver(observer);
}

void FrameSinkManagerHost::RemoveObserver(FrameSinkObserver* observer) {
  observers_.RemoveObserver(observer);
}

void FrameSinkManagerHost::CreateCompositorFrameSink(
    const cc::FrameSinkId& frame_sink_id,
    cc::mojom::MojoCompositorFrameSinkRequest request,
    cc::mojom::MojoCompositorFrameSinkPrivateRequest private_request,
    cc::mojom::MojoCompositorFrameSinkClientPtr client) {
  DCHECK(frame_sink_manager_ptr_.is_bound());
  frame_sink_manager_ptr_->CreateCompositorFrameSink(
      frame_sink_id, std::move(request), std::move(private_request),
      std::move(client));
}

void FrameSinkManagerHost::RegisterFrameSinkHierarchy(
    const cc::FrameSinkId& parent_frame_sink_id,
    const cc::FrameSinkId& child_frame_sink_id) {
  DCHECK(frame_sink_manager_ptr_.is_bound());
  frame_sink_manager_ptr_->RegisterFrameSinkHierarchy(parent_frame_sink_id,
                                                      child_frame_sink_id);
}

void FrameSinkManagerHost::UnregisterFrameSinkHierarchy(
    const cc::FrameSinkId& parent_frame_sink_id,
    const cc::FrameSinkId& child_frame_sink_id) {
  DCHECK(frame_sink_manager_ptr_.is_bound());
  frame_sink_manager_ptr_->UnregisterFrameSinkHierarchy(parent_frame_sink_id,
                                                        child_frame_sink_id);
}

void FrameSinkManagerHost::OnSurfaceCreated(
    const cc::SurfaceInfo& surface_info) {
  for (auto& observer : observers_)
    observer.OnSurfaceCreated(surface_info);
}

}  // namespace viz
