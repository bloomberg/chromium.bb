// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/host/ash_window_tree_host.h"

#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

AshWindowTreeHost::AshWindowTreeHost() : input_method_handler_(nullptr) {
}

void AshWindowTreeHost::TranslateLocatedEvent(ui::LocatedEvent* event) {
  if (event->IsTouchEvent())
    return;

  aura::WindowTreeHost* wth = AsWindowTreeHost();
  aura::Window* root_window = wth->window();
  aura::client::ScreenPositionClient* screen_position_client =
      aura::client::GetScreenPositionClient(root_window);
  gfx::Rect local(wth->GetBounds().size());
  local.Inset(GetHostInsets());

  if (screen_position_client && !local.Contains(event->location())) {
    gfx::Point location(event->location());
    // In order to get the correct point in screen coordinates
    // during passive grab, we first need to find on which host window
    // the mouse is on, and find out the screen coordinates on that
    // host window, then convert it back to this host window's coordinate.
    screen_position_client->ConvertHostPointToScreen(root_window, &location);
    screen_position_client->ConvertPointFromScreen(root_window, &location);
    wth->ConvertPointToHost(&location);
    event->set_location(location);
    event->set_root_location(location);
  }
}

}  // namespace ash
