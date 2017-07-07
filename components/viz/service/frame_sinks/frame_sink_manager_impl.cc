// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"

#include <utility>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "cc/base/switches.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/surfaces/display.h"
#include "components/viz/service/display_embedder/display_provider.h"
#include "components/viz/service/frame_sinks/gpu_compositor_frame_sink.h"
#include "components/viz/service/frame_sinks/gpu_root_compositor_frame_sink.h"

namespace viz {

FrameSinkManagerImpl::FrameSinkManagerImpl(bool use_surface_references,
                                           DisplayProvider* display_provider)
    : manager_(use_surface_references
                   ? cc::SurfaceManager::LifetimeType::REFERENCES
                   : cc::SurfaceManager::LifetimeType::SEQUENCES),
      display_provider_(display_provider),
      binding_(this) {
  manager_.surface_manager()->AddObserver(this);
}

FrameSinkManagerImpl::~FrameSinkManagerImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  manager_.surface_manager()->RemoveObserver(this);
}

void FrameSinkManagerImpl::BindPtrAndSetClient(
    cc::mojom::FrameSinkManagerRequest request,
    cc::mojom::FrameSinkManagerClientPtr client) {
  DCHECK(!binding_.is_bound());
  binding_.Bind(std::move(request));
  client_ = std::move(client);
}

void FrameSinkManagerImpl::CreateRootCompositorFrameSink(
    const cc::FrameSinkId& frame_sink_id,
    gpu::SurfaceHandle surface_handle,
    cc::mojom::CompositorFrameSinkAssociatedRequest request,
    cc::mojom::CompositorFrameSinkPrivateRequest private_request,
    cc::mojom::CompositorFrameSinkClientPtr client,
    cc::mojom::DisplayPrivateAssociatedRequest display_private_request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_NE(surface_handle, gpu::kNullSurfaceHandle);
  DCHECK_EQ(0u, compositor_frame_sinks_.count(frame_sink_id));
  DCHECK(display_provider_);

  std::unique_ptr<cc::BeginFrameSource> begin_frame_source;
  std::unique_ptr<cc::Display> display = display_provider_->CreateDisplay(
      frame_sink_id, surface_handle, &begin_frame_source);

  compositor_frame_sinks_[frame_sink_id] =
      base::MakeUnique<GpuRootCompositorFrameSink>(
          this, &manager_, frame_sink_id, std::move(display),
          std::move(begin_frame_source), std::move(request),
          std::move(private_request), std::move(client),
          std::move(display_private_request));
}

void FrameSinkManagerImpl::CreateCompositorFrameSink(
    const cc::FrameSinkId& frame_sink_id,
    cc::mojom::CompositorFrameSinkRequest request,
    cc::mojom::CompositorFrameSinkPrivateRequest private_request,
    cc::mojom::CompositorFrameSinkClientPtr client) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(0u, compositor_frame_sinks_.count(frame_sink_id));

  compositor_frame_sinks_[frame_sink_id] =
      base::MakeUnique<GpuCompositorFrameSink>(
          this, &manager_, frame_sink_id, std::move(request),
          std::move(private_request), std::move(client));
}

void FrameSinkManagerImpl::RegisterFrameSinkHierarchy(
    const cc::FrameSinkId& parent_frame_sink_id,
    const cc::FrameSinkId& child_frame_sink_id) {
  manager_.RegisterFrameSinkHierarchy(parent_frame_sink_id,
                                      child_frame_sink_id);
}

void FrameSinkManagerImpl::UnregisterFrameSinkHierarchy(
    const cc::FrameSinkId& parent_frame_sink_id,
    const cc::FrameSinkId& child_frame_sink_id) {
  manager_.UnregisterFrameSinkHierarchy(parent_frame_sink_id,
                                        child_frame_sink_id);
}

void FrameSinkManagerImpl::DropTemporaryReference(
    const cc::SurfaceId& surface_id) {
  manager_.DropTemporaryReference(surface_id);
}

void FrameSinkManagerImpl::DestroyCompositorFrameSink(cc::FrameSinkId sink_id) {
  compositor_frame_sinks_.erase(sink_id);
}

void FrameSinkManagerImpl::OnSurfaceCreated(
    const cc::SurfaceInfo& surface_info) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_GT(surface_info.device_scale_factor(), 0.0f);

  // TODO(kylechar): |client_| will try to find an owner for the temporary
  // reference to the new surface. With surface synchronization this might not
  // be necessary, because a surface reference might already exist and no
  // temporary reference was created. It could be useful to let |client_| know
  // if it should find an owner.
  if (client_)
    client_->OnSurfaceCreated(surface_info);
}

bool FrameSinkManagerImpl::OnSurfaceDamaged(const cc::SurfaceId& surface_id,
                                            const cc::BeginFrameAck& ack) {
  return false;
}

void FrameSinkManagerImpl::OnSurfaceDiscarded(const cc::SurfaceId& surface_id) {
}

void FrameSinkManagerImpl::OnSurfaceDestroyed(const cc::SurfaceId& surface_id) {
}

void FrameSinkManagerImpl::OnSurfaceDamageExpected(
    const cc::SurfaceId& surface_id,
    const cc::BeginFrameArgs& args) {}

void FrameSinkManagerImpl::OnSurfaceWillDraw(const cc::SurfaceId& surface_id) {}

void FrameSinkManagerImpl::OnClientConnectionLost(
    const cc::FrameSinkId& frame_sink_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (client_)
    client_->OnClientConnectionClosed(frame_sink_id);
}

void FrameSinkManagerImpl::OnPrivateConnectionLost(
    const cc::FrameSinkId& frame_sink_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DestroyCompositorFrameSink(frame_sink_id);
}

}  // namespace viz
