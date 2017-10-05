// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"

#include <stddef.h>
#include <stdint.h>

#include "base/logging.h"
#include "components/viz/service/display/display.h"
#include "components/viz/service/display_embedder/display_provider.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_impl.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_client.h"
#include "components/viz/service/frame_sinks/primary_begin_frame_source.h"
#include "components/viz/service/frame_sinks/root_compositor_frame_sink_impl.h"

#if DCHECK_IS_ON()
#include <sstream>
#endif

namespace viz {

FrameSinkManagerImpl::FrameSinkSourceMapping::FrameSinkSourceMapping() =
    default;

FrameSinkManagerImpl::FrameSinkSourceMapping::FrameSinkSourceMapping(
    const FrameSinkSourceMapping& other) = default;

FrameSinkManagerImpl::FrameSinkSourceMapping::~FrameSinkSourceMapping() =
    default;

FrameSinkManagerImpl::FrameSinkManagerImpl(
    SurfaceManager::LifetimeType lifetime_type,
    DisplayProvider* display_provider)
    : display_provider_(display_provider),
      surface_manager_(lifetime_type),
      hit_test_manager_(this),
      binding_(this) {
  surface_manager_.AddObserver(&hit_test_manager_);
  surface_manager_.AddObserver(this);
}

FrameSinkManagerImpl::~FrameSinkManagerImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // All FrameSinks should be unregistered prior to FrameSinkManager
  // destruction.
  compositor_frame_sinks_.clear();
  DCHECK_EQ(clients_.size(), 0u);
  DCHECK_EQ(registered_sources_.size(), 0u);
  surface_manager_.RemoveObserver(this);
  surface_manager_.RemoveObserver(&hit_test_manager_);
}

void FrameSinkManagerImpl::BindAndSetClient(
    mojom::FrameSinkManagerRequest request,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    mojom::FrameSinkManagerClientPtr client) {
  DCHECK(!client_);
  DCHECK(!binding_.is_bound());
  binding_.Bind(std::move(request), std::move(task_runner));
  client_ptr_ = std::move(client);

  client_ = client_ptr_.get();
}

void FrameSinkManagerImpl::SetLocalClient(
    mojom::FrameSinkManagerClient* client) {
  DCHECK(!client_ptr_);

  client_ = client;
}

void FrameSinkManagerImpl::RegisterFrameSinkId(
    const FrameSinkId& frame_sink_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  surface_manager_.RegisterFrameSinkId(frame_sink_id);
}

void FrameSinkManagerImpl::InvalidateFrameSinkId(
    const FrameSinkId& frame_sink_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  compositor_frame_sinks_.erase(frame_sink_id);
  surface_manager_.InvalidateFrameSinkId(frame_sink_id);
}

void FrameSinkManagerImpl::SetFrameSinkDebugLabel(
    const FrameSinkId& frame_sink_id,
    const std::string& debug_label) {
  surface_manager_.SetFrameSinkDebugLabel(frame_sink_id, debug_label);
}

void FrameSinkManagerImpl::CreateRootCompositorFrameSink(
    const FrameSinkId& frame_sink_id,
    gpu::SurfaceHandle surface_handle,
    const RendererSettings& renderer_settings,
    mojom::CompositorFrameSinkAssociatedRequest request,
    mojom::CompositorFrameSinkClientPtr client,
    mojom::DisplayPrivateAssociatedRequest display_private_request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_NE(surface_handle, gpu::kNullSurfaceHandle);
  DCHECK_EQ(0u, compositor_frame_sinks_.count(frame_sink_id));
  DCHECK(display_provider_);

  std::unique_ptr<BeginFrameSource> begin_frame_source;
  auto display = display_provider_->CreateDisplay(
      frame_sink_id, surface_handle, renderer_settings, &begin_frame_source);

  compositor_frame_sinks_[frame_sink_id] =
      base::MakeUnique<RootCompositorFrameSinkImpl>(
          this, frame_sink_id, std::move(display),
          std::move(begin_frame_source), std::move(request), std::move(client),
          std::move(display_private_request));
}

void FrameSinkManagerImpl::CreateCompositorFrameSink(
    const FrameSinkId& frame_sink_id,
    mojom::CompositorFrameSinkRequest request,
    mojom::CompositorFrameSinkClientPtr client) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(0u, compositor_frame_sinks_.count(frame_sink_id));

  compositor_frame_sinks_[frame_sink_id] =
      base::MakeUnique<CompositorFrameSinkImpl>(
          this, frame_sink_id, std::move(request), std::move(client));
}

void FrameSinkManagerImpl::RegisterFrameSinkHierarchy(
    const FrameSinkId& parent_frame_sink_id,
    const FrameSinkId& child_frame_sink_id) {
  // If it's possible to reach the parent through the child's descendant chain,
  // then this will create an infinite loop.  Might as well just crash here.
  CHECK(!ChildContains(child_frame_sink_id, parent_frame_sink_id));

  std::vector<FrameSinkId>& children =
      frame_sink_source_map_[parent_frame_sink_id].children;
  for (size_t i = 0; i < children.size(); ++i)
    DCHECK(children[i] != child_frame_sink_id);
  children.push_back(child_frame_sink_id);

  // If the parent has no source, then attaching it to this child will
  // not change any downstream sources.
  BeginFrameSource* parent_source =
      frame_sink_source_map_[parent_frame_sink_id].source;
  if (!parent_source)
    return;

  DCHECK_EQ(registered_sources_.count(parent_source), 1u);
  RecursivelyAttachBeginFrameSource(child_frame_sink_id, parent_source);
}

void FrameSinkManagerImpl::UnregisterFrameSinkHierarchy(
    const FrameSinkId& parent_frame_sink_id,
    const FrameSinkId& child_frame_sink_id) {
  // Deliberately do not check validity of either parent or child
  // FrameSinkId here.  They were valid during the registration, so were
  // valid at some point in time.  This makes it possible to invalidate parent
  // and child FrameSinkIds independently of each other and not have an ordering
  // dependency  of unregistering the hierarchy first before either of them.
  DCHECK_EQ(frame_sink_source_map_.count(parent_frame_sink_id), 1u);

  auto iter = frame_sink_source_map_.find(parent_frame_sink_id);

  std::vector<FrameSinkId>& children = iter->second.children;
  bool found_child = false;
  for (size_t i = 0; i < children.size(); ++i) {
    if (children[i] == child_frame_sink_id) {
      found_child = true;
      children[i] = children.back();
      children.resize(children.size() - 1);
      break;
    }
  }
  DCHECK(found_child);

  // The CompositorFrameSinkSupport and hierarchy can be registered/unregistered
  // in either order, so empty frame_sink_source_map entries need to be
  // checked when removing either clients or relationships.
  if (!iter->second.has_children() && !clients_.count(parent_frame_sink_id) &&
      !iter->second.source) {
    frame_sink_source_map_.erase(iter);
    return;
  }

  // If the parent does not have a begin frame source, then disconnecting it
  // will not change any of its children.
  BeginFrameSource* parent_source = iter->second.source;
  if (!parent_source)
    return;

  // TODO(enne): these walks could be done in one step.
  RecursivelyDetachBeginFrameSource(child_frame_sink_id, parent_source);
  for (auto source_iter : registered_sources_)
    RecursivelyAttachBeginFrameSource(source_iter.second, source_iter.first);
}

void FrameSinkManagerImpl::AssignTemporaryReference(const SurfaceId& surface_id,
                                                    const FrameSinkId& owner) {
  surface_manager_.AssignTemporaryReference(surface_id, owner);
}

void FrameSinkManagerImpl::DropTemporaryReference(const SurfaceId& surface_id) {
  surface_manager_.DropTemporaryReference(surface_id);
}

void FrameSinkManagerImpl::RegisterFrameSinkManagerClient(
    const FrameSinkId& frame_sink_id,
    FrameSinkManagerClient* client) {
  DCHECK(client);

  clients_[frame_sink_id] = client;

  auto it = frame_sink_source_map_.find(frame_sink_id);
  if (it != frame_sink_source_map_.end()) {
    if (it->second.source)
      client->SetBeginFrameSource(it->second.source);
  }
}

void FrameSinkManagerImpl::UnregisterFrameSinkManagerClient(
    const FrameSinkId& frame_sink_id) {
  auto client_iter = clients_.find(frame_sink_id);
  DCHECK(client_iter != clients_.end());

  auto source_iter = frame_sink_source_map_.find(frame_sink_id);
  if (source_iter != frame_sink_source_map_.end()) {
    if (source_iter->second.source)
      client_iter->second->SetBeginFrameSource(nullptr);
  }
  clients_.erase(client_iter);
}

void FrameSinkManagerImpl::RegisterBeginFrameSource(
    BeginFrameSource* source,
    const FrameSinkId& frame_sink_id) {
  DCHECK(source);
  DCHECK_EQ(registered_sources_.count(source), 0u);

  registered_sources_[source] = frame_sink_id;
  RecursivelyAttachBeginFrameSource(frame_sink_id, source);

  primary_source_.OnBeginFrameSourceAdded(source);
}

void FrameSinkManagerImpl::UnregisterBeginFrameSource(
    BeginFrameSource* source) {
  DCHECK(source);
  DCHECK_EQ(registered_sources_.count(source), 1u);

  FrameSinkId frame_sink_id = registered_sources_[source];
  registered_sources_.erase(source);

  primary_source_.OnBeginFrameSourceRemoved(source);

  if (frame_sink_source_map_.count(frame_sink_id) == 0u)
    return;

  // TODO(enne): these walks could be done in one step.
  // Remove this begin frame source from its subtree.
  RecursivelyDetachBeginFrameSource(frame_sink_id, source);
  // Then flush every remaining registered source to fix any sources that
  // became null because of the previous step but that have an alternative.
  for (auto source_iter : registered_sources_)
    RecursivelyAttachBeginFrameSource(source_iter.second, source_iter.first);
}

BeginFrameSource* FrameSinkManagerImpl::GetPrimaryBeginFrameSource() {
  return &primary_source_;
}

void FrameSinkManagerImpl::RecursivelyAttachBeginFrameSource(
    const FrameSinkId& frame_sink_id,
    BeginFrameSource* source) {
  FrameSinkSourceMapping& mapping = frame_sink_source_map_[frame_sink_id];
  if (!mapping.source) {
    mapping.source = source;
    auto client_iter = clients_.find(frame_sink_id);
    if (client_iter != clients_.end())
      client_iter->second->SetBeginFrameSource(source);
  }
  for (size_t i = 0; i < mapping.children.size(); ++i) {
    // |frame_sink_source_map_| is a container that can allocate new memory and
    // move data between buffers. Copy child's FrameSinkId before passing
    // it to RecursivelyAttachBeginFrameSource so that we don't reference data
    // inside |frame_sink_source_map_|.
    FrameSinkId child_copy = mapping.children[i];
    RecursivelyAttachBeginFrameSource(child_copy, source);
  }
}

void FrameSinkManagerImpl::RecursivelyDetachBeginFrameSource(
    const FrameSinkId& frame_sink_id,
    BeginFrameSource* source) {
  auto iter = frame_sink_source_map_.find(frame_sink_id);
  if (iter == frame_sink_source_map_.end())
    return;
  if (iter->second.source == source) {
    iter->second.source = nullptr;
    auto client_iter = clients_.find(frame_sink_id);
    if (client_iter != clients_.end())
      client_iter->second->SetBeginFrameSource(nullptr);
  }

  if (!iter->second.has_children() && !clients_.count(frame_sink_id)) {
    frame_sink_source_map_.erase(iter);
    return;
  }

  std::vector<FrameSinkId>& children = iter->second.children;
  for (size_t i = 0; i < children.size(); ++i) {
    RecursivelyDetachBeginFrameSource(children[i], source);
  }
}

bool FrameSinkManagerImpl::ChildContains(
    const FrameSinkId& child_frame_sink_id,
    const FrameSinkId& search_frame_sink_id) const {
  auto iter = frame_sink_source_map_.find(child_frame_sink_id);
  if (iter == frame_sink_source_map_.end())
    return false;

  const std::vector<FrameSinkId>& children = iter->second.children;
  for (size_t i = 0; i < children.size(); ++i) {
    if (children[i] == search_frame_sink_id)
      return true;
    if (ChildContains(children[i], search_frame_sink_id))
      return true;
  }
  return false;
}

void FrameSinkManagerImpl::OnFirstSurfaceActivation(
    const SurfaceInfo& surface_info) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_GT(surface_info.device_scale_factor(), 0.0f);

  // TODO(kylechar): |client_| will try to find an owner for the temporary
  // reference to the new surface. With surface synchronization this might not
  // be necessary, because a surface reference might already exist and no
  // temporary reference was created. It could be useful to let |client_| know
  // if it should find an owner.
  if (client_)
    client_->OnFirstSurfaceActivation(surface_info);
}

void FrameSinkManagerImpl::OnSurfaceActivated(const SurfaceId& surface_id) {}

bool FrameSinkManagerImpl::OnSurfaceDamaged(const SurfaceId& surface_id,
                                            const BeginFrameAck& ack) {
  return false;
}

void FrameSinkManagerImpl::OnSurfaceDiscarded(const SurfaceId& surface_id) {}

void FrameSinkManagerImpl::OnSurfaceDestroyed(const SurfaceId& surface_id) {}

void FrameSinkManagerImpl::OnSurfaceDamageExpected(const SurfaceId& surface_id,
                                                   const BeginFrameArgs& args) {
}

void FrameSinkManagerImpl::OnSurfaceWillDraw(const SurfaceId& surface_id) {}

void FrameSinkManagerImpl::OnClientConnectionLost(
    const FrameSinkId& frame_sink_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (client_)
    client_->OnClientConnectionClosed(frame_sink_id);
}

void FrameSinkManagerImpl::SubmitHitTestRegionList(
    const SurfaceId& surface_id,
    uint64_t frame_index,
    mojom::HitTestRegionListPtr hit_test_region_list) {
  hit_test_manager_.SubmitHitTestRegionList(surface_id, frame_index,
                                            std::move(hit_test_region_list));
}

uint64_t FrameSinkManagerImpl::GetActiveFrameIndex(
    const SurfaceId& surface_id) {
  return surface_manager_.GetSurfaceForId(surface_id)->GetActiveFrameIndex();
}

void FrameSinkManagerImpl::OnAggregatedHitTestRegionListUpdated(
    const FrameSinkId& frame_sink_id,
    mojo::ScopedSharedBufferHandle active_handle,
    uint32_t active_handle_size,
    mojo::ScopedSharedBufferHandle idle_handle,
    uint32_t idle_handle_size) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (client_) {
    client_->OnAggregatedHitTestRegionListUpdated(
        frame_sink_id, std::move(active_handle), active_handle_size,
        std::move(idle_handle), idle_handle_size);
  }
}

void FrameSinkManagerImpl::SwitchActiveAggregatedHitTestRegionList(
    const FrameSinkId& frame_sink_id,
    uint8_t active_handle_index) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (client_) {
    client_->SwitchActiveAggregatedHitTestRegionList(frame_sink_id,
                                                     active_handle_index);
  }
}

}  // namespace viz
