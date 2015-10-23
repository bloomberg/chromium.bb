// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_WINDOW_FINDER_H_
#define COMPONENTS_MUS_WS_WINDOW_FINDER_H_

namespace cc {
struct SurfaceId;
}

namespace gfx {
class Point;
class Transform;
}

namespace mus {
namespace ws {

class ServerWindow;

// Find the deepest visible child of |root| that contains |location|. If a
// child is found |location| is reset to the coordinates of the child.
ServerWindow* FindDeepestVisibleWindow(ServerWindow* root,
                                       gfx::Point* location);

// Find the deepest visible child of |root| that contains |location| using
// a previously submitted frame.
ServerWindow* FindDeepestVisibleWindowFromSurface(
    ServerWindow* root_window,
    cc::SurfaceId display_surface_id,
    const gfx::Point& location,
    gfx::Transform* transform);

// Retrieve the transform to the provided |target_window|'s coordinate
// space.
void GetTransformToTargetWindow(ServerWindow* target_window,
                                gfx::Transform* transform);

// Retrieve the transform to the provided |target_window|'s coordinate
// space. Returns whether a transform was successfully retrieved.
bool GetTransformToTargetWindowFromSurface(cc::SurfaceId display_surface_id,
                                           ServerWindow* target_window,
                                           gfx::Transform* transform);
}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_WINDOW_FINDER_H_
