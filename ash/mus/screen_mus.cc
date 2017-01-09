// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/screen_mus.h"

#include "ui/aura/env.h"
#include "ui/aura/mus/window_tree_host_mus.h"
#include "ui/aura/window.h"

namespace ash {

ScreenMus::ScreenMus() = default;

ScreenMus::~ScreenMus() = default;

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

}  // namespace ash
