// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_SERVER_WINDOW_DELEGATE_H_
#define COMPONENTS_MUS_WS_SERVER_WINDOW_DELEGATE_H_

#include "base/memory/scoped_ptr.h"
#include "components/mus/public/interfaces/mus_constants.mojom.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"

namespace mus {

class SurfacesState;

namespace ws {

struct ClientWindowId;
class ServerWindow;
struct WindowId;

class ServerWindowDelegate {
 public:
  virtual SurfacesState* GetSurfacesState() = 0;

  virtual void OnScheduleWindowPaint(const ServerWindow* window) = 0;

  // Returns the root of the window tree to which this |window| is attached.
  // Returns null if this window is not attached up through to a root window.
  virtual const ServerWindow* GetRootWindow(
      const ServerWindow* window) const = 0;

  // Schedules a callback to DestroySurfacesScheduledForDestruction() at the
  // appropriate time, which may be synchronously.
  virtual void ScheduleSurfaceDestruction(ServerWindow* window) = 0;

  // Used to resolve a reference to a Window by ClientWindowId in submitted
  // frames. When the client owning |ancestor| (or the client embedded at
  // |ancestor|) submits a frame the frame may referenced child windows. The
  // reference is done using an id that is only known to the client. This
  // function resolves the id into the appropriate window, or null if a window
  // can't be found.
  virtual ServerWindow* FindWindowForSurface(
      const ServerWindow* ancestor,
      mojom::SurfaceType surface_type,
      const ClientWindowId& client_window_id) = 0;

 protected:
  virtual ~ServerWindowDelegate() {}
};

}  // namespace ws

}  // namespace mus

#endif  // COMPONENTS_MUS_WS_SERVER_WINDOW_DELEGATE_H_
