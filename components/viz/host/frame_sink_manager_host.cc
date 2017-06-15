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
    cc::mojom::MojoCompositorFrameSinkClientPtr client) {
  DCHECK_EQ(frame_sink_data_map_.count(frame_sink_id), 0u);

  FrameSinkData& data = frame_sink_data_map_[frame_sink_id];

  frame_sink_manager_ptr_->CreateCompositorFrameSink(
      frame_sink_id, std::move(request),
      mojo::MakeRequest(&data.private_interface), std::move(client));
}

void FrameSinkManagerHost::DestroyCompositorFrameSink(
    const cc::FrameSinkId& frame_sink_id) {
  auto iter = frame_sink_data_map_.find(frame_sink_id);
  DCHECK(iter != frame_sink_data_map_.end());

  FrameSinkData& data = iter->second;
  if (data.parent.has_value())
    UnregisterFrameSinkHierarchy(data.parent.value(), frame_sink_id);

  // This destroys the MojoCompositorFrameSinkPrivatePtr and closes the pipe.
  frame_sink_data_map_.erase(iter);
}

void FrameSinkManagerHost::RegisterFrameSinkHierarchy(
    const cc::FrameSinkId& parent_frame_sink_id,
    const cc::FrameSinkId& child_frame_sink_id) {
  // Register and store the parent.
  frame_sink_manager_ptr_->RegisterFrameSinkHierarchy(parent_frame_sink_id,
                                                      child_frame_sink_id);
  frame_sink_data_map_[child_frame_sink_id].parent = parent_frame_sink_id;
}

void FrameSinkManagerHost::UnregisterFrameSinkHierarchy(
    const cc::FrameSinkId& parent_frame_sink_id,
    const cc::FrameSinkId& child_frame_sink_id) {
  auto iter = frame_sink_data_map_.find(child_frame_sink_id);
  DCHECK(iter != frame_sink_data_map_.end());

  FrameSinkData& data = iter->second;
  DCHECK_EQ(data.parent.value(), parent_frame_sink_id);

  // Unregister and clear the stored parent.
  frame_sink_manager_ptr_->UnregisterFrameSinkHierarchy(parent_frame_sink_id,
                                                        child_frame_sink_id);
  data.parent.reset();

  // If the client never called CreateCompositorFrameSink() then they won't
  // call DestroyCompositorFrameSink() either, so cleanup map entry here.
  if (!data.private_interface.is_bound())
    frame_sink_data_map_.erase(iter);
}

void FrameSinkManagerHost::OnSurfaceCreated(
    const cc::SurfaceInfo& surface_info) {
  for (auto& observer : observers_)
    observer.OnSurfaceCreated(surface_info);
}

FrameSinkManagerHost::FrameSinkData::FrameSinkData() = default;

FrameSinkManagerHost::FrameSinkData::FrameSinkData(FrameSinkData&& other) =
    default;

FrameSinkManagerHost::FrameSinkData::~FrameSinkData() = default;

FrameSinkManagerHost::FrameSinkData& FrameSinkManagerHost::FrameSinkData::
operator=(FrameSinkData&& other) = default;

}  // namespace viz
