// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_SERVER_WINDOW_SURFACE_MANAGER_H_
#define COMPONENTS_MUS_WS_SERVER_WINDOW_SURFACE_MANAGER_H_

#include "base/containers/scoped_ptr_map.h"
#include "base/macros.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "components/mus/public/interfaces/compositor_frame.mojom.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace mus {
namespace ws {

class ServerWindow;
class ServerWindowSurface;

// ServerWindowSurfaceManager tracks the surfaces associated with a
// ServerWindow.
class ServerWindowSurfaceManager {
 public:
  explicit ServerWindowSurfaceManager(ServerWindow* window);
  ~ServerWindowSurfaceManager();

  // Creates a new surface of the specified type, replacing the existing one of
  // the specified type.
  void CreateSurface(mojom::SurfaceType surface_type,
                     mojo::InterfaceRequest<mojom::Surface> request,
                     mojom::SurfaceClientPtr client);

  ServerWindow* window() { return window_; }

  ServerWindowSurface* GetDefaultSurface();
  ServerWindowSurface* GetUnderlaySurface();
  ServerWindowSurface* GetSurfaceByType(mojom::SurfaceType type);

 private:
  friend class ServerWindowSurface;

  cc::SurfaceId GenerateId();

  ServerWindow* window_;

  cc::SurfaceIdAllocator surface_id_allocator_;

  using TypeToSurfaceMap =
      base::ScopedPtrMap<mojom::SurfaceType, scoped_ptr<ServerWindowSurface>>;

  TypeToSurfaceMap type_to_surface_map_;

  DISALLOW_COPY_AND_ASSIGN(ServerWindowSurfaceManager);
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_SERVER_WINDOW_SURFACE_MANAGER_H_
