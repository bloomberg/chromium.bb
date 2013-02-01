// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/tabs/dock_info_ash.h"

#include "ash/shell_window_ids.h"
#include "ash/wm/coordinate_conversion.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"

namespace {

aura::Window* GetLocalProcessWindowAtPointImpl(
    const gfx::Point& screen_point,
    const std::set<gfx::NativeView>& ignore,
    aura::Window* window) {
  if (ignore.find(window) != ignore.end())
    return NULL;

  if (!window->IsVisible())
    return NULL;

  if (window->id() == ash::internal::kShellWindowId_PhantomWindow ||
      window->id() == ash::internal::kShellWindowId_OverlayContainer)
    return NULL;

  if (window->layer()->type() == ui::LAYER_TEXTURED)
    return window->GetBoundsInScreen().Contains(screen_point) ? window : NULL;

  for (aura::Window::Windows::const_reverse_iterator i =
           window->children().rbegin(); i != window->children().rend(); ++i) {
    aura::Window* result =
        GetLocalProcessWindowAtPointImpl(screen_point, ignore, *i);
    if (result)
      return result;
  }
  return NULL;
}

}  // namespace

namespace chrome {
namespace ash {

DockInfo GetDockInfoAtPointAsh(
    const gfx::Point& screen_point, const std::set<gfx::NativeView>& ignore) {
  // TODO(beng):
  NOTIMPLEMENTED();
  return DockInfo();
}

gfx::NativeView GetLocalProcessWindowAtPointAsh(
    const gfx::Point& screen_point,
    const std::set<gfx::NativeView>& ignore) {
  return GetLocalProcessWindowAtPointImpl(
      screen_point, ignore, ::ash::wm::GetRootWindowAt(screen_point));
}

}  // namespace ash
}  // namespace chrome
