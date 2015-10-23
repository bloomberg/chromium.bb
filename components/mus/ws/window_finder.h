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

// Find the deepest visible child of |root| that contains |location| using
// a previously submitted frame. |location| is initially in the coordinate
// space of |root_window|. On return it is converted to the coordinates of the
// return value.
ServerWindow* FindDeepestVisibleWindow(ServerWindow* root_window,
                                       cc::SurfaceId display_surface_id,
                                       gfx::Point* location);

// Retrieve the transform to the provided |window|'s coordinate space from the
// root.
gfx::Transform GetTransformToWindow(cc::SurfaceId display_surface_id,
                                    ServerWindow* window);

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_WINDOW_FINDER_H_
