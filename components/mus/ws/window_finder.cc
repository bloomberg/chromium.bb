// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/window_finder.h"

#include "cc/surfaces/surface_id.h"
#include "components/mus/surfaces/surfaces_state.h"
#include "components/mus/ws/server_window.h"
#include "components/mus/ws/server_window_delegate.h"
#include "components/mus/ws/window_coordinate_conversions.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/transform.h"

namespace mus {
namespace ws {
namespace {

ServerWindow* FindDeepestVisibleWindowNonSurface(ServerWindow* window,
                                                 gfx::Point* location) {
  for (ServerWindow* child : window->GetChildren()) {
    if (!child->visible())
      continue;

    // TODO(sky): support transform.
    gfx::Point child_location(location->x() - child->bounds().x(),
                              location->y() - child->bounds().y());
    if (child_location.x() >= 0 && child_location.y() >= 0 &&
        child_location.x() < child->bounds().width() &&
        child_location.y() < child->bounds().height()) {
      *location = child_location;
      return FindDeepestVisibleWindowNonSurface(child, location);
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

}  // namespace

ServerWindow* FindDeepestVisibleWindow(ServerWindow* root_window,
                                       cc::SurfaceId display_surface_id,
                                       gfx::Point* location) {
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
  gfx::Transform transform;
  if (!display_surface_id.is_null() &&
      window->delegate()
          ->GetSurfacesState()
          ->hit_tester()
          ->GetTransformToTargetSurface(display_surface_id,
                                        window->surface()->id(), &transform)) {
    return transform;
  }

  return GetTransformToWindowNonSurface(window);
}

}  // namespace ws
}  // namespace mus
