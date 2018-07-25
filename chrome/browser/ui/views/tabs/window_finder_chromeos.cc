// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/window_finder.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/wm/root_window_finder.h"  // mash-ok
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/mus/mus_client.h"

namespace {

gfx::NativeWindow GetLocalProcessWindowAtPointAsh(
    const gfx::Point& screen_point,
    const std::set<gfx::NativeWindow>& ignore,
    gfx::NativeWindow window) {
  if (ignore.find(window) != ignore.end())
    return nullptr;

  if (!window->IsVisible())
    return nullptr;

  if (window->id() == ash::kShellWindowId_PhantomWindow ||
      window->id() == ash::kShellWindowId_OverlayContainer ||
      window->id() == ash::kShellWindowId_MouseCursorContainer)
    return nullptr;

  if (window->layer()->type() == ui::LAYER_TEXTURED) {
    // Returns the window that has visible layer and can hit the
    // |screen_point|, because we want to detach the tab as soon as
    // the dragging mouse moved over to the window that can hide the
    // moving tab.
    aura::client::ScreenPositionClient* client =
        aura::client::GetScreenPositionClient(window->GetRootWindow());
    gfx::Point local_point = screen_point;
    client->ConvertPointFromScreen(window, &local_point);
    return window->GetEventHandlerForPoint(local_point) ? window : nullptr;
  }

  for (aura::Window::Windows::const_reverse_iterator i =
           window->children().rbegin();
       i != window->children().rend(); ++i) {
    gfx::NativeWindow result =
        GetLocalProcessWindowAtPointAsh(screen_point, ignore, *i);
    if (result)
      return result;
  }
  return nullptr;
}

gfx::NativeWindow GetLocalProcessWindowAtPointMus(
    const gfx::Point& screen_point,
    const std::set<gfx::NativeWindow>& ignore) {
  std::set<aura::Window*> root_windows =
      views::MusClient::Get()->window_tree_client()->GetRoots();
  // TODO(erg): Needs to deal with stacking order here.

  // For every mus window, look at the associated aura window and see if we're
  // in that.
  for (aura::Window* root : root_windows) {
    views::Widget* widget = views::Widget::GetWidgetForNativeView(root);
    if (widget && widget->GetWindowBoundsInScreen().Contains(screen_point)) {
      aura::Window* content_window = widget->GetNativeWindow();

      // If we were instructed to ignore this window, ignore it.
      if (base::ContainsKey(ignore, content_window))
        continue;

      return content_window;
    }
  }

  return nullptr;
}

}  // namespace

gfx::NativeWindow WindowFinder::GetLocalProcessWindowAtPoint(
    const gfx::Point& screen_point,
    const std::set<gfx::NativeWindow>& ignore) {
  if (!features::IsAshInBrowserProcess())
    return GetLocalProcessWindowAtPointMus(screen_point, ignore);

  return GetLocalProcessWindowAtPointAsh(
      screen_point, ignore, ash::wm::GetRootWindowAt(screen_point));
}
