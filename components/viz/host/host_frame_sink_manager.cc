// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/host/host_frame_sink_manager.h"

#include <utility>

#include "base/sequenced_task_runner.h"
#include "cc/surfaces/surface_info.h"
#include "cc/surfaces/surface_manager.h"

namespace viz {

HostFrameSinkManager::HostFrameSinkManager() : binding_(this) {}

HostFrameSinkManager::~HostFrameSinkManager() = default;

void HostFrameSinkManager::BindAndSetManager(
    cc::mojom::FrameSinkManagerClientRequest request,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    cc::mojom::FrameSinkManagerPtr ptr) {
  DCHECK(!binding_.is_bound());
  binding_.Bind(std::move(request), std::move(task_runner));
  frame_sink_manager_ptr_ = std::move(ptr);
}

void HostFrameSinkManager::AddObserver(FrameSinkObserver* observer) {
  observers_.AddObserver(observer);
}

void HostFrameSinkManager::RemoveObserver(FrameSinkObserver* observer) {
  observers_.RemoveObserver(observer);
}

void HostFrameSinkManager::CreateCompositorFrameSink(
    const FrameSinkId& frame_sink_id,
    cc::mojom::CompositorFrameSinkRequest request,
    cc::mojom::CompositorFrameSinkClientPtr client) {
  DCHECK_EQ(frame_sink_data_map_.count(frame_sink_id), 0u);

  FrameSinkData& data = frame_sink_data_map_[frame_sink_id];

  frame_sink_manager_ptr_->CreateCompositorFrameSink(
      frame_sink_id, std::move(request),
      mojo::MakeRequest(&data.private_interface), std::move(client));
}

void HostFrameSinkManager::DestroyCompositorFrameSink(
    const FrameSinkId& frame_sink_id) {
  auto iter = frame_sink_data_map_.find(frame_sink_id);
  DCHECK(iter != frame_sink_data_map_.end());

  FrameSinkData& data = iter->second;
  if (data.parent.has_value())
    UnregisterFrameSinkHierarchy(data.parent.value(), frame_sink_id);

  // This destroys the CompositorFrameSinkPrivatePtr and closes the pipe.
  frame_sink_data_map_.erase(iter);
}

void HostFrameSinkManager::RegisterFrameSinkHierarchy(
    const FrameSinkId& parent_frame_sink_id,
    const FrameSinkId& child_frame_sink_id) {
  // Register and store the parent.
  frame_sink_manager_ptr_->RegisterFrameSinkHierarchy(parent_frame_sink_id,
                                                      child_frame_sink_id);
  frame_sink_data_map_[child_frame_sink_id].parent = parent_frame_sink_id;
}

void HostFrameSinkManager::UnregisterFrameSinkHierarchy(
    const FrameSinkId& parent_frame_sink_id,
    const FrameSinkId& child_frame_sink_id) {
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

void HostFrameSinkManager::OnSurfaceCreated(
    const cc::SurfaceInfo& surface_info) {
  for (auto& observer : observers_)
    observer.OnSurfaceCreated(surface_info);
}

void HostFrameSinkManager::OnClientConnectionClosed(
    const FrameSinkId& frame_sink_id) {
  // TODO(kylechar): Notify observers.
}

HostFrameSinkManager::FrameSinkData::FrameSinkData() = default;

HostFrameSinkManager::FrameSinkData::FrameSinkData(FrameSinkData&& other) =
    default;

HostFrameSinkManager::FrameSinkData::~FrameSinkData() = default;

HostFrameSinkManager::FrameSinkData& HostFrameSinkManager::FrameSinkData::
operator=(FrameSinkData&& other) = default;

}  // namespace viz
