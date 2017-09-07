// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/host/host_frame_sink_manager.h"

#include <utility>

#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
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

void HostFrameSinkManager::RegisterFrameSinkId(const FrameSinkId& frame_sink_id,
                                               HostFrameSinkClient* client) {
  DCHECK(frame_sink_id.is_valid());
  DCHECK(client);

  FrameSinkData& data = frame_sink_data_map_[frame_sink_id];
  DCHECK(!data.IsFrameSinkRegistered());
  DCHECK(!data.HasCompositorFrameSinkData());
  data.client = client;
  frame_sink_manager_->RegisterFrameSinkId(frame_sink_id);
}

void HostFrameSinkManager::InvalidateFrameSinkId(
    const FrameSinkId& frame_sink_id) {
  DCHECK(frame_sink_id.is_valid());

  FrameSinkData& data = frame_sink_data_map_[frame_sink_id];
  DCHECK(data.IsFrameSinkRegistered());

  // This will destroy |frame_sink_id| if using mojom::CompositorFrameSink.
  frame_sink_manager_->InvalidateFrameSinkId(frame_sink_id);
  data.has_created_compositor_frame_sink = false;
  data.client = nullptr;

  // There may be frame sink hierarchy information left in FrameSinkData.
  if (data.IsEmpty())
    frame_sink_data_map_.erase(frame_sink_id);

  display_hit_test_query_.erase(frame_sink_id);
}

void HostFrameSinkManager::SetFrameSinkDebugLabel(
    const FrameSinkId& frame_sink_id,
    const std::string& debug_label) {
  frame_sink_manager_->SetFrameSinkDebugLabel(frame_sink_id, debug_label);
}

void HostFrameSinkManager::CreateRootCompositorFrameSink(
    const FrameSinkId& frame_sink_id,
    gpu::SurfaceHandle surface_handle,
    const RendererSettings& renderer_settings,
    mojom::CompositorFrameSinkAssociatedRequest request,
    mojom::CompositorFrameSinkClientPtr client,
    mojom::DisplayPrivateAssociatedRequest display_private_request) {
  FrameSinkData& data = frame_sink_data_map_[frame_sink_id];
  DCHECK(data.IsFrameSinkRegistered());
  DCHECK(!data.HasCompositorFrameSinkData());

  data.is_root = true;
  data.has_created_compositor_frame_sink = true;

  frame_sink_manager_->CreateRootCompositorFrameSink(
      frame_sink_id, surface_handle, renderer_settings, std::move(request),
      std::move(client), std::move(display_private_request));
  display_hit_test_query_[frame_sink_id] = base::MakeUnique<HitTestQuery>();
}

void HostFrameSinkManager::CreateCompositorFrameSink(
    const FrameSinkId& frame_sink_id,
    mojom::CompositorFrameSinkRequest request,
    mojom::CompositorFrameSinkClientPtr client) {
  FrameSinkData& data = frame_sink_data_map_[frame_sink_id];
  DCHECK(data.IsFrameSinkRegistered());
  DCHECK(!data.HasCompositorFrameSinkData());

  data.is_root = false;
  data.has_created_compositor_frame_sink = true;

  frame_sink_manager_->CreateCompositorFrameSink(
      frame_sink_id, std::move(request), std::move(client));
}

void HostFrameSinkManager::RegisterFrameSinkHierarchy(
    const FrameSinkId& parent_frame_sink_id,
    const FrameSinkId& child_frame_sink_id) {
  // Register and store the parent.
  frame_sink_manager_->RegisterFrameSinkHierarchy(parent_frame_sink_id,
                                                  child_frame_sink_id);

  FrameSinkData& child_data = frame_sink_data_map_[child_frame_sink_id];
  DCHECK(!base::ContainsValue(child_data.parents, parent_frame_sink_id));
  child_data.parents.push_back(parent_frame_sink_id);

  FrameSinkData& parent_data = frame_sink_data_map_[parent_frame_sink_id];
  DCHECK(!base::ContainsValue(parent_data.children, child_frame_sink_id));
  parent_data.children.push_back(child_frame_sink_id);
}

void HostFrameSinkManager::UnregisterFrameSinkHierarchy(
    const FrameSinkId& parent_frame_sink_id,
    const FrameSinkId& child_frame_sink_id) {
  // Unregister and clear the stored parent.
  FrameSinkData& child_data = frame_sink_data_map_[child_frame_sink_id];
  DCHECK(base::ContainsValue(child_data.parents, parent_frame_sink_id));
  base::Erase(child_data.parents, parent_frame_sink_id);

  FrameSinkData& parent_data = frame_sink_data_map_[parent_frame_sink_id];
  DCHECK(base::ContainsValue(parent_data.children, child_frame_sink_id));
  base::Erase(parent_data.children, child_frame_sink_id);

  frame_sink_manager_->UnregisterFrameSinkHierarchy(parent_frame_sink_id,
                                                    child_frame_sink_id);

  if (child_data.IsEmpty())
    frame_sink_data_map_.erase(child_frame_sink_id);
  if (parent_data.IsEmpty())
    frame_sink_data_map_.erase(parent_frame_sink_id);
}

void HostFrameSinkManager::AssignTemporaryReference(
    const SurfaceId& surface_id,
    const FrameSinkId& frame_sink_id) {
  frame_sink_manager_->AssignTemporaryReference(surface_id, frame_sink_id);
}

void HostFrameSinkManager::DropTemporaryReference(const SurfaceId& surface_id) {
  frame_sink_manager_->DropTemporaryReference(surface_id);
}

std::unique_ptr<CompositorFrameSinkSupport>
HostFrameSinkManager::CreateCompositorFrameSinkSupport(
    CompositorFrameSinkSupportClient* client,
    const FrameSinkId& frame_sink_id,
    bool is_root,
    bool needs_sync_points) {
  DCHECK(frame_sink_manager_impl_);

  FrameSinkData& data = frame_sink_data_map_[frame_sink_id];
  DCHECK(data.IsFrameSinkRegistered());
  DCHECK(!data.HasCompositorFrameSinkData());

  auto support = CompositorFrameSinkSupport::Create(
      client, frame_sink_manager_impl_, frame_sink_id, is_root,
      needs_sync_points);
  support->SetDestructionCallback(
      base::BindOnce(&HostFrameSinkManager::CompositorFrameSinkSupportDestroyed,
                     weak_ptr_factory_.GetWeakPtr(), frame_sink_id));

  data.support = support.get();
  data.is_root = is_root;

  return support;
}

void HostFrameSinkManager::CompositorFrameSinkSupportDestroyed(
    const FrameSinkId& frame_sink_id) {
  auto iter = frame_sink_data_map_.find(frame_sink_id);
  DCHECK(iter != frame_sink_data_map_.end());

  iter->second.support = nullptr;
  if (iter->second.IsEmpty())
    frame_sink_data_map_.erase(iter);
}

void HostFrameSinkManager::PerformAssignTemporaryReference(
    const SurfaceId& surface_id) {
  // Find the expected embedder for the new surface and assign the temporary
  // reference to it.
  auto iter = frame_sink_data_map_.find(surface_id.frame_sink_id());
  DCHECK(iter != frame_sink_data_map_.end());
  const FrameSinkData& data = iter->second;

  // Display roots don't have temporary references to assign.
  if (data.is_root)
    return;

  // If the frame sink has already been invalidated then we just drop the
  // temporary reference.
  if (!data.IsFrameSinkRegistered()) {
    frame_sink_manager_->DropTemporaryReference(surface_id);
    return;
  }

  // Find the oldest non-invalidated parent.
  for (const FrameSinkId& parent_id : data.parents) {
    const FrameSinkData& parent_data = frame_sink_data_map_[parent_id];
    if (parent_data.IsFrameSinkRegistered()) {
      frame_sink_manager_impl_->AssignTemporaryReference(surface_id, parent_id);
      return;
    }
  }
  // TODO(kylechar): We might need to handle the case where there are multiple
  // embedders better, so that the owner doesn't remove a surface reference
  // until the other embedder has added a surface reference. Maybe just letting
  // the client know what is the owner is sufficient, so the client can handle
  // this.

  // We don't have any hierarchy information for what will embed the new
  // surface, drop the temporary reference.
  frame_sink_manager_->DropTemporaryReference(surface_id);
}

void HostFrameSinkManager::OnFirstSurfaceActivation(
    const SurfaceInfo& surface_info) {
  auto it = frame_sink_data_map_.find(surface_info.id().frame_sink_id());
  // If we've received a bogus or stale SurfaceId from Viz then just ignore it.
  if (it == frame_sink_data_map_.end()) {
    // We don't have any hierarchy information for what will embed the new
    // surface, drop the temporary reference.
    frame_sink_manager_->DropTemporaryReference(surface_info.id());
    return;
  }

  FrameSinkData& frame_sink_data = it->second;
  if (frame_sink_data.client)
    frame_sink_data.client->OnFirstSurfaceActivation(surface_info);

  if (frame_sink_manager_impl_ &&
      frame_sink_manager_impl_->surface_manager()->using_surface_references()) {
    PerformAssignTemporaryReference(surface_info.id());
  }
}

void HostFrameSinkManager::OnClientConnectionClosed(
    const FrameSinkId& frame_sink_id) {
  // TODO(kylechar): Notify observers.
}

void HostFrameSinkManager::OnAggregatedHitTestRegionListUpdated(
    const FrameSinkId& frame_sink_id,
    mojo::ScopedSharedBufferHandle active_handle,
    uint32_t active_handle_size,
    mojo::ScopedSharedBufferHandle idle_handle,
    uint32_t idle_handle_size) {
  auto iter = display_hit_test_query_.find(frame_sink_id);
  if (iter == display_hit_test_query_.end()) {
    // TODO(riajiang): Report security fault. http://crbug.com/746470
    // Or verify if it is the case that display got destroyed, but viz doesn't
    // know it yet.
    NOTREACHED();
    return;
  }
  iter->second->OnAggregatedHitTestRegionListUpdated(
      std::move(active_handle), active_handle_size, std::move(idle_handle),
      idle_handle_size);
}

void HostFrameSinkManager::SwitchActiveAggregatedHitTestRegionList(
    const FrameSinkId& frame_sink_id,
    uint8_t active_handle_index) {
  auto iter = display_hit_test_query_.find(frame_sink_id);
  if (iter == display_hit_test_query_.end() ||
      (active_handle_index != 0u && active_handle_index != 1u)) {
    // TODO(riajiang): Report security fault. http://crbug.com/746470
    // Or verify if it is the case that display got destroyed, but viz doesn't
    // know it yet.
    NOTREACHED();
    return;
  }
  iter->second->SwitchActiveAggregatedHitTestRegionList(active_handle_index);
}

HostFrameSinkManager::FrameSinkData::FrameSinkData() = default;

HostFrameSinkManager::FrameSinkData::FrameSinkData(FrameSinkData&& other) =
    default;

HostFrameSinkManager::FrameSinkData::~FrameSinkData() = default;

HostFrameSinkManager::FrameSinkData& HostFrameSinkManager::FrameSinkData::
operator=(FrameSinkData&& other) = default;

}  // namespace viz
