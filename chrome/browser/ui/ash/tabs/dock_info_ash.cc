// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/dock_info.h"

#include "ash/shell_window_ids.h"
#include "ash/wm/coordinate_conversion.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"

// DockInfo -------------------------------------------------------------------

namespace {

aura::Window* GetLocalProcessWindowAtPointImpl(
    const gfx::Point& screen_point,
    const std::set<gfx::NativeView>& ignore,
    aura::Window* window) {
  if (ignore.find(window) != ignore.end())
    return NULL;

  if (!window->IsVisible())
    return NULL;

  if (window->id() == ash::internal::kShellWindowId_PhantomWindow)
    return NULL;

  if (window->layer()->type() == ui::LAYER_TEXTURED) {
    gfx::Point window_point(screen_point);
    aura::client::GetScreenPositionClient(window->GetRootWindow())->
        ConvertPointFromScreen(window, &window_point);
    return gfx::Rect(window->bounds().size()).Contains(window_point) ?
        window : NULL;
  }
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

// static
DockInfo DockInfo::GetDockInfoAtPoint(const gfx::Point& screen_point,
                                      const std::set<gfx::NativeView>& ignore) {
  // TODO(beng):
  NOTIMPLEMENTED();
  return DockInfo();
}

// static
gfx::NativeView DockInfo::GetLocalProcessWindowAtPoint(
    const gfx::Point& screen_point,
    const std::set<gfx::NativeView>& ignore) {
  return GetLocalProcessWindowAtPointImpl(
      screen_point, ignore, ash::wm::GetRootWindowAt(screen_point));
}

bool DockInfo::GetWindowBounds(gfx::Rect* bounds) const {
  if (!window())
    return false;
  *bounds = window_->bounds();
  return true;
}

void DockInfo::SizeOtherWindowTo(const gfx::Rect& bounds) const {
  window_->SetBounds(bounds);
}
