// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/window_finder.h"

#include "cc/surfaces/surface_id.h"
#include "components/mus/surfaces/surfaces_state.h"
#include "components/mus/ws/server_window.h"
#include "components/mus/ws/server_window_delegate.h"
#include "components/mus/ws/server_window_surface.h"
#include "components/mus/ws/server_window_surface_manager.h"
#include "components/mus/ws/window_coordinate_conversions.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/transform.h"

namespace mus {
namespace ws {
namespace {

bool IsValidWindowForEvents(ServerWindow* window) {
  ServerWindowSurfaceManager* surface_manager = window->surface_manager();
  return surface_manager &&
         surface_manager->HasSurfaceOfType(mojom::SurfaceType::DEFAULT);
}

ServerWindow* FindDeepestVisibleWindowNonSurface(ServerWindow* window,
                                                 gfx::Point* location) {
  const ServerWindow::Windows children(window->GetChildren());
  for (auto iter = children.rbegin(); iter != children.rend(); ++iter) {
    ServerWindow* child = *iter;
    if (!child->visible())
      continue;

    // TODO(sky): support transform.
    gfx::Point child_location(location->x() - child->bounds().x(),
                              location->y() - child->bounds().y());
    gfx::Rect child_bounds(child->bounds().size());
    child_bounds.Inset(-child->extended_hit_test_region().left(),
                       -child->extended_hit_test_region().top(),
                       -child->extended_hit_test_region().right(),
                       -child->extended_hit_test_region().bottom());
    if (child_bounds.Contains(child_location)) {
      *location = child_location;
      ServerWindow* result =
          FindDeepestVisibleWindowNonSurface(child, location);
      if (IsValidWindowForEvents(result))
        return result;
    }
  }
  return window;
}

gfx::Transform GetTransformToWindowNonSurface(ServerWindow* window) {
  gfx::Transform transform;
  ServerWindow* current = window;
  while (current->parent()) {
    transform.Translate(-current->bounds().x(), -current->bounds().y());
    current = current->parent();
  }
  return transform;
}

bool HitTestSurfaceOfType(cc::SurfaceId display_surface_id,
                          ServerWindow* window,
                          mus::mojom::SurfaceType surface_type,
                          gfx::Transform* transform) {
  *transform = gfx::Transform();
  ServerWindowSurface* surface =
      window->GetOrCreateSurfaceManager()->GetSurfaceByType(surface_type);
  return surface &&
         window->delegate()
             ->GetSurfacesState()
             ->hit_tester()
             ->GetTransformToTargetSurface(display_surface_id, surface->id(),
                                           transform);
}

}  // namespace

ServerWindow* FindDeepestVisibleWindowForEvents(
    ServerWindow* root_window,
    cc::SurfaceId display_surface_id,
    gfx::Point* location) {
  // TODO(sky): remove this when insets can be set on surface.
  display_surface_id = cc::SurfaceId();

  if (display_surface_id.is_null()) {
    // Surface-based hit-testing will not return a valid target if no
    // CompositorFrame has been submitted (e.g. in unit-tests).
    return FindDeepestVisibleWindowNonSurface(root_window, location);
  }

  gfx::Transform transform;
  cc::SurfaceId target_surface =
      root_window->delegate()
          ->GetSurfacesState()
          ->hit_tester()
          ->GetTargetSurfaceAtPoint(display_surface_id, *location, &transform);
  WindowId id = WindowIdFromTransportId(
      cc::SurfaceIdAllocator::NamespaceForId(target_surface));
  // TODO(fsamuel): This should be a DCHECK but currently we use stale
  // information to decide where to route input events. This should be fixed
  // once we implement a UI scheduler.
  ServerWindow* target = root_window->GetChildWindow(id);
  if (target)
    transform.TransformPoint(location);
  return target;
}

gfx::Transform GetTransformToWindow(cc::SurfaceId display_surface_id,
                                    ServerWindow* window) {
  if (!display_surface_id.is_null()) {
    gfx::Transform transform;
    if (HitTestSurfaceOfType(display_surface_id, window,
                             mus::mojom::SurfaceType::DEFAULT, &transform) ||
        HitTestSurfaceOfType(display_surface_id, window,
                             mus::mojom::SurfaceType::UNDERLAY, &transform)) {
      return transform;
    }
  }

  return GetTransformToWindowNonSurface(window);
}

}  // namespace ws
}  // namespace mus
