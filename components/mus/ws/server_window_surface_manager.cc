// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/server_window_surface_manager.h"

#include "components/mus/ws/server_window.h"
#include "components/mus/ws/server_window_delegate.h"
#include "components/mus/ws/server_window_surface.h"

namespace mus {
namespace ws {

ServerWindowSurfaceManager::ServerWindowSurfaceManager(ServerWindow* window)
    : window_(window),
      surface_id_allocator_(WindowIdToTransportId(window->id())),
      waiting_for_initial_frames_(
          window_->properties().count(mus::mojom::kWaitForUnderlay_Property) >
          0) {}

ServerWindowSurfaceManager::~ServerWindowSurfaceManager() {}

bool ServerWindowSurfaceManager::ShouldDraw() {
  if (!waiting_for_initial_frames_)
    return true;

  waiting_for_initial_frames_ =
      !IsSurfaceReadyAndNonEmpty(mojom::SURFACE_TYPE_DEFAULT) ||
      !IsSurfaceReadyAndNonEmpty(mojom::SURFACE_TYPE_UNDERLAY);
  return !waiting_for_initial_frames_;
}

void ServerWindowSurfaceManager::CreateSurface(
    mojom::SurfaceType surface_type,
    mojo::InterfaceRequest<mojom::Surface> request,
    mojom::SurfaceClientPtr client) {
  type_to_surface_map_[surface_type] = make_scoped_ptr(new ServerWindowSurface(
      this, surface_type, std::move(request), std::move(client)));
}

ServerWindowSurface* ServerWindowSurfaceManager::GetDefaultSurface() {
  return GetSurfaceByType(mojom::SURFACE_TYPE_DEFAULT);
}

ServerWindowSurface* ServerWindowSurfaceManager::GetUnderlaySurface() {
  return GetSurfaceByType(mojom::SURFACE_TYPE_UNDERLAY);
}

ServerWindowSurface* ServerWindowSurfaceManager::GetSurfaceByType(
    mojom::SurfaceType type) {
  auto iter = type_to_surface_map_.find(type);
  return iter == type_to_surface_map_.end() ? nullptr : iter->second.get();
}

bool ServerWindowSurfaceManager::HasSurfaceOfType(mojom::SurfaceType type) {
  return type_to_surface_map_.count(type) > 0;
}

bool ServerWindowSurfaceManager::IsSurfaceReadyAndNonEmpty(
    mojom::SurfaceType type) const {
  auto iter = type_to_surface_map_.find(type);
  if (iter == type_to_surface_map_.end())
    return false;
  if (iter->second->last_submitted_frame_size().IsEmpty())
    return false;
  const gfx::Size& last_submitted_frame_size =
      iter->second->last_submitted_frame_size();
  return last_submitted_frame_size.width() >= window_->bounds().width() &&
         last_submitted_frame_size.height() >= window_->bounds().height();
}

cc::SurfaceId ServerWindowSurfaceManager::GenerateId() {
  return surface_id_allocator_.GenerateId();
}

}  // namespace ws
}  // namespace mus
