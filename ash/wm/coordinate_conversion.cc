// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/coordinate_conversion.h"

#include "ash/display/display_controller.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/root_window.h"
#include "ui/gfx/display.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"

namespace ash {
namespace wm {

aura::RootWindow* GetRootWindowAt(const gfx::Point& point) {
  const gfx::Display& display = gfx::Screen::GetDisplayNearestPoint(point);
  // TODO(yusukes): Move coordinate_conversion.cc and .h to ui/aura/ once
  // GetRootWindowForDisplayId() is moved to aura::Env.
  return Shell::GetInstance()->display_controller()->
      GetRootWindowForDisplayId(display.id());
}

aura::RootWindow* GetRootWindowMatching(const gfx::Rect& rect) {
  const gfx::Display& display = gfx::Screen::GetDisplayMatching(rect);
  return Shell::GetInstance()->display_controller()->
      GetRootWindowForDisplayId(display.id());
}

std::pair<aura::RootWindow*, gfx::Point> GetRootWindowRelativeToWindow(
    aura::Window* window,
    const gfx::Point& location) {
  aura::RootWindow* root_window = window->GetRootWindow();
  gfx::Point location_in_root(location);
  aura::Window::ConvertPointToTarget(window, root_window, &location_in_root);

#if defined(USE_X11)
  if (!root_window->ContainsPointInRoot(location_in_root)) {
    // This is to translate an out of bounds mouse pointer location in
    // its root window coordinate into correct screen coordinates.
    // There are two scenarios that a target root window receives an
    // out of bounds mouse event.
    //
    // 1) When aura's input is captured by an aura window, and the
    //  mouse pointer is on another root window. Typical example is
    //  when you open a menu and move the mouse to another display (which is
    //  another root window). X's mouse event is sent to the window
    //  where mouse is on, but the aura event is sent to the window
    //  that has a capture, which is on a different root window.
    //  This is covered in the is_valid block below.
    //
    // 2) When X's native input is captured by a drag operation.  When
    //  you press a button on a window, X automatically grabs the
    //  mouse input (passive grab
    //  http://www.x.org/wiki/development/documentation/grabprocessing)
    //  and keeps sending mouse events to the window where the drag
    //  operation started until the mouse button is released. This is
    //  covered in the else block below.

    gfx::Point location_in_screen = location_in_root;
    aura::client::ScreenPositionClient* client =
        aura::client::GetScreenPositionClient(root_window);
    client->ConvertPointToScreen(root_window, &location_in_screen);
    gfx::Display display =
        ScreenAsh::FindDisplayContainingPoint(location_in_screen);
    if (display.is_valid()) {
      // This conversion is necessary to warp the mouse pointer
      // correctly while aura's input capture is in effect. In this
      // case, the location is already translated relative to the root
      // (but it's outside of the root window), so all we have to do
      // is to find a root window containing the point in screen
      // coordinate.
      aura::RootWindow* root = Shell::GetInstance()->display_controller()->
          GetRootWindowForDisplayId(display.id());
      DCHECK(root);
      client->ConvertPointFromScreen(root, &location_in_screen);
      root_window = root;
      location_in_root = location_in_screen;
    } else {
      // This conversion is necessary to deal with X's passive input
      // grab while dragging window. For example, if we have two
      // displays, say 1000x1000 (primary) and 500x500 (extended one
      // on the right), and start dragging a window at (999, 123), and
      // then move the pointer to the right, the pointer suddenly
      // warps to the extended display. The destination is (0, 123) in
      // the secondary root window's coordinates, or (1000, 123) in
      // the screen coordinates. However, since the mouse is captured
      // by X during drag, a weird LocatedEvent, something like (0, 1123)
      // in the *primary* root window's coordinates, is sent to Chrome
      // (Remember that in the native X11 world, the two root windows
      // are always stacked vertically regardless of the display
      // layout in Ash). We need to figure out that (0, 1123) in the
      // primary root window's coordinates is actually (0, 123) in the
      // extended root window's coordinates.

      gfx::Point location_in_native(location_in_root);
      root_window->ConvertPointToNativeScreen(&location_in_native);

      Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
      for (size_t i = 0; i < root_windows.size(); ++i) {
        const gfx::Rect native_bounds(
            root_windows[i]->GetHostOrigin(),
            root_windows[i]->GetHostSize());  // in px.
        if (native_bounds.Contains(location_in_native)) {
          root_window = root_windows[i];
          location_in_root = location_in_native;
          root_window->ConvertPointFromNativeScreen(&location_in_root);
          break;
        }
      }
    }
  }
#else
  // TODO(yusukes): Support non-X11 platforms if necessary.
#endif

  return std::make_pair(root_window, location_in_root);
}

void ConvertPointToScreen(aura::Window* window, gfx::Point* point) {
  aura::client::GetScreenPositionClient(window->GetRootWindow())->
      ConvertPointToScreen(window, point);
}

void ConvertPointFromScreen(aura::Window* window,
                            gfx::Point* point_in_screen) {
  aura::client::GetScreenPositionClient(window->GetRootWindow())->
      ConvertPointFromScreen(window, point_in_screen);
}

}  // namespace wm
}  // namespace ash
