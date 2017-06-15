// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/mojo_frame_sink_manager.h"

#include <utility>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "cc/base/switches.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/surfaces/display.h"
#include "cc/surfaces/surface_dependency_tracker.h"
#include "components/viz/service/display_compositor/display_provider.h"
#include "components/viz/service/frame_sinks/gpu_compositor_frame_sink.h"
#include "components/viz/service/frame_sinks/gpu_root_compositor_frame_sink.h"

namespace viz {

MojoFrameSinkManager::MojoFrameSinkManager(bool use_surface_references,
                                           DisplayProvider* display_provider)
    : manager_(use_surface_references
                   ? cc::SurfaceManager::LifetimeType::REFERENCES
                   : cc::SurfaceManager::LifetimeType::SEQUENCES),
      display_provider_(display_provider),
      binding_(this) {
  manager_.AddObserver(this);
  dependency_tracker_ = base::MakeUnique<cc::SurfaceDependencyTracker>(
      &manager_, manager_.GetPrimaryBeginFrameSource());
  manager_.SetDependencyTracker(dependency_tracker_.get());
}

MojoFrameSinkManager::~MojoFrameSinkManager() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  manager_.SetDependencyTracker(nullptr);
  dependency_tracker_.reset();
  manager_.RemoveObserver(this);
}

void MojoFrameSinkManager::BindPtrAndSetClient(
    cc::mojom::FrameSinkManagerRequest request,
    cc::mojom::FrameSinkManagerClientPtr client) {
  DCHECK(!binding_.is_bound());
  binding_.Bind(std::move(request));
  client_ = std::move(client);
}

void MojoFrameSinkManager::CreateRootCompositorFrameSink(
    const cc::FrameSinkId& frame_sink_id,
    gpu::SurfaceHandle surface_handle,
    cc::mojom::MojoCompositorFrameSinkAssociatedRequest request,
    cc::mojom::MojoCompositorFrameSinkPrivateRequest private_request,
    cc::mojom::MojoCompositorFrameSinkClientPtr client,
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

void MojoFrameSinkManager::CreateCompositorFrameSink(
    const cc::FrameSinkId& frame_sink_id,
    cc::mojom::MojoCompositorFrameSinkRequest request,
    cc::mojom::MojoCompositorFrameSinkPrivateRequest private_request,
    cc::mojom::MojoCompositorFrameSinkClientPtr client) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(0u, compositor_frame_sinks_.count(frame_sink_id));

  compositor_frame_sinks_[frame_sink_id] =
      base::MakeUnique<GpuCompositorFrameSink>(
          this, &manager_, frame_sink_id, std::move(request),
          std::move(private_request), std::move(client));
}

void MojoFrameSinkManager::RegisterFrameSinkHierarchy(
    const cc::FrameSinkId& parent_frame_sink_id,
    const cc::FrameSinkId& child_frame_sink_id) {
  manager_.RegisterFrameSinkHierarchy(parent_frame_sink_id,
                                      child_frame_sink_id);
}

void MojoFrameSinkManager::UnregisterFrameSinkHierarchy(
    const cc::FrameSinkId& parent_frame_sink_id,
    const cc::FrameSinkId& child_frame_sink_id) {
  manager_.UnregisterFrameSinkHierarchy(parent_frame_sink_id,
                                        child_frame_sink_id);
}

void MojoFrameSinkManager::DropTemporaryReference(
    const cc::SurfaceId& surface_id) {
  manager_.DropTemporaryReference(surface_id);
}

void MojoFrameSinkManager::DestroyCompositorFrameSink(cc::FrameSinkId sink_id) {
  compositor_frame_sinks_.erase(sink_id);
}

void MojoFrameSinkManager::OnSurfaceCreated(
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

bool MojoFrameSinkManager::OnSurfaceDamaged(const cc::SurfaceId& surface_id,
                                            const cc::BeginFrameAck& ack) {
  return false;
}

void MojoFrameSinkManager::OnSurfaceDiscarded(const cc::SurfaceId& surface_id) {
}

void MojoFrameSinkManager::OnSurfaceDestroyed(const cc::SurfaceId& surface_id) {
}

void MojoFrameSinkManager::OnSurfaceDamageExpected(
    const cc::SurfaceId& surface_id,
    const cc::BeginFrameArgs& args) {}

void MojoFrameSinkManager::OnClientConnectionLost(
    const cc::FrameSinkId& frame_sink_id,
    bool destroy_compositor_frame_sink) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (destroy_compositor_frame_sink)
    DestroyCompositorFrameSink(frame_sink_id);
  // TODO(fsamuel): Tell the frame sink manager host that the client connection
  // has been lost so that it can drop its private connection and allow a new
  // client instance to create a new CompositorFrameSink.
}

void MojoFrameSinkManager::OnSurfaceWillDraw(const cc::SurfaceId& surface_id) {}

void MojoFrameSinkManager::OnPrivateConnectionLost(
    const cc::FrameSinkId& frame_sink_id,
    bool destroy_compositor_frame_sink) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (destroy_compositor_frame_sink)
    DestroyCompositorFrameSink(frame_sink_id);
}

}  // namespace viz
