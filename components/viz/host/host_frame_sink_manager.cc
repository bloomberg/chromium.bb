// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/host/host_frame_sink_manager.h"

#include <utility>

#include "base/sequenced_task_runner.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support_client.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"

namespace viz {

HostFrameSinkManager::HostFrameSinkManager()
    : binding_(this), weak_ptr_factory_(this) {}

HostFrameSinkManager::~HostFrameSinkManager() = default;

void HostFrameSinkManager::SetLocalManager(
    FrameSinkManagerImpl* frame_sink_manager_impl) {
  DCHECK(!frame_sink_manager_ptr_);
  frame_sink_manager_impl_ = frame_sink_manager_impl;

  frame_sink_manager_ = frame_sink_manager_impl;
}

void HostFrameSinkManager::BindAndSetManager(
    mojom::FrameSinkManagerClientRequest request,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    mojom::FrameSinkManagerPtr ptr) {
  DCHECK(!frame_sink_manager_impl_);
  DCHECK(!binding_.is_bound());

  binding_.Bind(std::move(request), std::move(task_runner));
  frame_sink_manager_ptr_ = std::move(ptr);

  frame_sink_manager_ = frame_sink_manager_ptr_.get();
}

void HostFrameSinkManager::AddObserver(FrameSinkObserver* observer) {
  observers_.AddObserver(observer);
}

void HostFrameSinkManager::RemoveObserver(FrameSinkObserver* observer) {
  observers_.RemoveObserver(observer);
}

void HostFrameSinkManager::CreateCompositorFrameSink(
    const FrameSinkId& frame_sink_id,
    mojom::CompositorFrameSinkRequest request,
    mojom::CompositorFrameSinkClientPtr client) {
  FrameSinkData& data = frame_sink_data_map_[frame_sink_id];
  DCHECK(!data.HasCompositorFrameSinkData());

  data.is_root = false;
  data.has_created_compositor_frame_sink = true;

  frame_sink_manager_->CreateCompositorFrameSink(
      frame_sink_id, std::move(request), std::move(client));
  frame_sink_manager_->RegisterFrameSinkId(frame_sink_id);
}

void HostFrameSinkManager::DestroyCompositorFrameSink(
    const FrameSinkId& frame_sink_id) {
  auto iter = frame_sink_data_map_.find(frame_sink_id);
  DCHECK(iter != frame_sink_data_map_.end());

  FrameSinkData& data = iter->second;
  DCHECK(data.HasCompositorFrameSinkData());
  if (data.has_created_compositor_frame_sink) {
    // This will also destroy the CompositorFrameSink pipe to the client.
    frame_sink_manager_->InvalidateFrameSinkId(frame_sink_id);
    data.has_created_compositor_frame_sink = false;
  } else {
    data.support = nullptr;
  }

  if (data.IsEmpty())
    frame_sink_data_map_.erase(iter);
}

void HostFrameSinkManager::RegisterFrameSinkHierarchy(
    const FrameSinkId& parent_frame_sink_id,
    const FrameSinkId& child_frame_sink_id) {
  // Register and store the parent.
  frame_sink_manager_->RegisterFrameSinkHierarchy(parent_frame_sink_id,
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
  data.parent.reset();

  // Unregister and clear the stored parent.
  frame_sink_manager_->UnregisterFrameSinkHierarchy(parent_frame_sink_id,
                                                    child_frame_sink_id);

  if (data.IsEmpty())
    frame_sink_data_map_.erase(iter);
}

std::unique_ptr<CompositorFrameSinkSupport>
HostFrameSinkManager::CreateCompositorFrameSinkSupport(
    CompositorFrameSinkSupportClient* client,
    const FrameSinkId& frame_sink_id,
    bool is_root,
    bool handles_frame_sink_id_invalidation,
    bool needs_sync_points) {
  DCHECK(frame_sink_manager_impl_);

  FrameSinkData& data = frame_sink_data_map_[frame_sink_id];
  DCHECK(!data.HasCompositorFrameSinkData());

  auto support = CompositorFrameSinkSupport::Create(
      client, frame_sink_manager_impl_, frame_sink_id, is_root,
      handles_frame_sink_id_invalidation, needs_sync_points);
  support->SetDestructionCallback(
      base::BindOnce(&HostFrameSinkManager::DestroyCompositorFrameSink,
                     weak_ptr_factory_.GetWeakPtr(), frame_sink_id));

  data.support = support.get();
  data.is_root = is_root;

  return support;
}

void HostFrameSinkManager::PerformAssignTemporaryReference(
    const SurfaceId& surface_id) {
  // Find the expected embedder for the new surface and assign the temporary
  // reference to it.
  auto iter = frame_sink_data_map_.find(surface_id.frame_sink_id());
  if (iter != frame_sink_data_map_.end()) {
    const FrameSinkData& data = iter->second;

    // Display roots don't have temporary references to assign.
    if (data.is_root)
      return;

    if (data.parent.has_value()) {
      frame_sink_manager_impl_->AssignTemporaryReference(surface_id,
                                                         data.parent.value());
      return;
    }
  }

  // We don't have any hierarchy information for what will embed the new
  // surface, drop the temporary reference.
  frame_sink_manager_->DropTemporaryReference(surface_id);
}

void HostFrameSinkManager::OnSurfaceCreated(const SurfaceInfo& surface_info) {
  for (auto& observer : observers_)
    observer.OnSurfaceCreated(surface_info);

  if (frame_sink_manager_impl_ &&
      frame_sink_manager_impl_->surface_manager()->using_surface_references()) {
    PerformAssignTemporaryReference(surface_info.id());
  }
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
