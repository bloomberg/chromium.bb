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
      surface_id_allocator_(WindowIdToTransportId(window->id())) {}

ServerWindowSurfaceManager::~ServerWindowSurfaceManager() {}

void ServerWindowSurfaceManager::CreateSurface(
    mojom::SurfaceType surface_type,
    mojo::InterfaceRequest<mojom::Surface> request,
    mojom::SurfaceClientPtr client) {
  type_to_surface_map_.set(
      surface_type, make_scoped_ptr(new ServerWindowSurface(
                        this, surface_type, request.Pass(), client.Pass())));
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
  return iter == type_to_surface_map_.end() ? nullptr : iter->second;
}

cc::SurfaceId ServerWindowSurfaceManager::GenerateId() {
  return surface_id_allocator_.GenerateId();
}

}  // namespace ws
}  // namespace mus
