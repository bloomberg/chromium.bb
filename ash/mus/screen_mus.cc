// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/screen_mus.h"

#include "ash/common/wm/root_window_finder.h"
#include "ash/common/wm_window.h"
#include "services/ui/public/interfaces/display/display_controller.mojom.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/window_tree_host_mus.h"
#include "ui/aura/window.h"

namespace ash {

ScreenMus::ScreenMus(display::mojom::DisplayController* display_controller)
    : display_controller_(display_controller) {}

ScreenMus::~ScreenMus() = default;

void ScreenMus::SetWorkAreaInsets(aura::Window* window,
                                  const gfx::Insets& insets) {
  // If we are not the active instance assume we're in shutdown, and ignore.
  if (display::Screen::GetScreen() != this)
    return;

  display::Display old_display = GetDisplayNearestWindow(window);
  display::Display new_display = old_display;
  new_display.UpdateWorkAreaFromInsets(insets);
  if (old_display.work_area() == new_display.work_area())
    return;

  display_list().UpdateDisplay(new_display);
  if (display_controller_) {
    display_controller_->SetDisplayWorkArea(
        new_display.id(), new_display.bounds().size(), insets);
  }
}

display::Display ScreenMus::GetDisplayNearestWindow(
    aura::Window* window) const {
  const aura::WindowTreeHost* host = window->GetHost();
  if (!host)
    return GetPrimaryDisplay();
  auto iter = display_list().FindDisplayById(
      static_cast<const aura::WindowTreeHostMus*>(host)->display_id());
  return iter == display_list().displays().end() ? GetPrimaryDisplay() : *iter;
}

gfx::Point ScreenMus::GetCursorScreenPoint() {
  return aura::Env::GetInstance()->last_mouse_location();
}

bool ScreenMus::IsWindowUnderCursor(gfx::NativeWindow window) {
  return GetWindowAtScreenPoint(GetCursorScreenPoint()) == window;
}

gfx::NativeWindow ScreenMus::GetWindowAtScreenPoint(const gfx::Point& point) {
  aura::Window* root_window =
      WmWindow::GetAuraWindow(wm::GetRootWindowAt(point));
  aura::client::ScreenPositionClient* position_client =
      aura::client::GetScreenPositionClient(root_window);

  gfx::Point local_point = point;
  if (position_client)
    position_client->ConvertPointFromScreen(root_window, &local_point);

  return root_window->GetTopWindowContainingPoint(local_point);
}

}  // namespace ash
