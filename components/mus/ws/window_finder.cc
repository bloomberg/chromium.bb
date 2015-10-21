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

namespace mus {
namespace ws {

ServerWindow* FindDeepestVisibleWindow(ServerWindow* window,
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
      return FindDeepestVisibleWindow(child, location);
    }
  }
  return window;
}

ServerWindow* FindDeepestVisibleWindowFromSurface(ServerWindow* window,
                                                  cc::SurfaceId surface_id,
                                                  gfx::Point* location) {
  if (surface_id.is_null())
    return nullptr;

  gfx::Transform transform_to_target_surface;
  cc::SurfaceId target_surface =
      window->delegate()
          ->GetSurfacesState()
          ->hit_tester()
          ->GetTargetSurfaceAtPoint(surface_id, *location,
                                    &transform_to_target_surface);
  WindowId id = WindowIdFromTransportId(
      cc::SurfaceIdAllocator::NamespaceForId(target_surface));
  ServerWindow* target = window->GetChildWindow(id);
  // TODO(fsamuel): This should be a DCHECK but currently we use stale
  // information to decide where to route input events. This should be fixed
  // once we implement a UI scheduler.
  if (target) {
    transform_to_target_surface.TransformPoint(location);
    return target;
  }
  return nullptr;
}

}  // namespace ws
}  // namespace mus
