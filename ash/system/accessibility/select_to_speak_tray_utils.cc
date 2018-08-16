// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/select_to_speak_tray_utils.h"

#include "ash/session/session_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/accessibility/select_to_speak_tray.h"
#include "ash/system/status_area_widget.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/point.h"

namespace ash {
namespace select_to_speak_tray_utils {

bool SelectToSpeakTrayContainsPointInScreen(const gfx::Point& point) {
  for (aura::Window* window : Shell::GetAllRootWindows()) {
    SelectToSpeakTray* tray =
        Shelf::ForWindow(window)->GetStatusAreaWidget()->select_to_speak_tray();
    if (tray && tray->ContainsPointInScreen(point))
      return true;
  }

  return false;
}

}  // namespace select_to_speak_tray_utils
}  // namespace ash