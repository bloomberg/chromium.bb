// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_finder.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/wm/root_window_finder.h"
#include "services/ui/ws2/window_service.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window.h"

namespace {

// Returns true if |window| is considered to be a toplevel window.
bool IsTopLevelWindow(aura::Window* window) {
  // ui::LAYER_TEXTURED is for non-mash environment. For Mash, browser windows
  // are not with LAYER_TEXTURED but have a remote client.
  return window->layer()->type() == ui::LAYER_TEXTURED ||
         ui::ws2::WindowService::HasRemoteClient(window);
}

// Get the toplevel window at |screen_point| among the descendants of |window|.
aura::Window* GetTopmostWindowAtPointWithinWindow(
    const gfx::Point& screen_point,
    aura::Window* window,
    const std::set<aura::Window*> ignore,
    aura::Window** real_topmost) {
  if (!window->IsVisible())
    return nullptr;

  if (window->id() == ash::kShellWindowId_PhantomWindow ||
      window->id() == ash::kShellWindowId_OverlayContainer ||
      window->id() == ash::kShellWindowId_MouseCursorContainer)
    return nullptr;

  if (IsTopLevelWindow(window)) {
    aura::client::ScreenPositionClient* client =
        aura::client::GetScreenPositionClient(window->GetRootWindow());
    gfx::Point local_point = screen_point;
    client->ConvertPointFromScreen(window, &local_point);
    if (window->GetEventHandlerForPoint(local_point)) {
      if (real_topmost && !(*real_topmost))
        *real_topmost = window;
      return (ignore.find(window) == ignore.end()) ? window : nullptr;
    }
    return nullptr;
  }

  for (aura::Window::Windows::const_reverse_iterator i =
           window->children().rbegin();
       i != window->children().rend(); ++i) {
    aura::Window* result = GetTopmostWindowAtPointWithinWindow(
        screen_point, *i, ignore, real_topmost);
    if (result)
      return result;
  }
  return nullptr;
}

}  // namespace

namespace ash {
namespace wm {

aura::Window* GetTopmostWindowAtPoint(const gfx::Point& screen_point,
                                      const std::set<aura::Window*>& ignore,
                                      aura::Window** real_topmost) {
  if (real_topmost)
    *real_topmost = nullptr;
  return GetTopmostWindowAtPointWithinWindow(
      screen_point, GetRootWindowAt(screen_point), ignore, real_topmost);
}

}  // namespace wm
}  // namespace ash
