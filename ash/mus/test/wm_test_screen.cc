// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/test/wm_test_screen.h"

#include "ui/aura/window.h"
#include "ui/display/display_finder.h"

namespace ash {
namespace mus {

WmTestScreen::WmTestScreen() {}
WmTestScreen::~WmTestScreen() {}

gfx::Point WmTestScreen::GetCursorScreenPoint() {
  // TODO(sky): use the implementation from WindowManagerConnection.
  DVLOG(1) << "NOTIMPLEMENTED";
  return gfx::Point();
}

bool WmTestScreen::IsWindowUnderCursor(gfx::NativeWindow window) {
  if (!window)
    return false;

  return window->IsVisible() &&
         window->GetBoundsInScreen().Contains(GetCursorScreenPoint());
}

}  // namespace mus
}  // namespace ash
