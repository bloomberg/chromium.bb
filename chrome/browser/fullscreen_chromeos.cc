// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fullscreen.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

bool IsFullScreenMode(int64_t display_id) {
  if (ash_util::IsRunningInMash()) {
    // TODO: http://crbug.com/640390.
    NOTIMPLEMENTED();
    return false;
  }

  for (ash::RootWindowController* controller :
       ash::Shell::GetInstance()->GetAllRootWindowControllers()) {
    if (display::Screen::GetScreen()
            ->GetDisplayNearestWindow(controller->GetRootWindow())
            .id() == display_id) {
      return controller && controller->GetWindowForFullscreenMode();
    }
  }

  return false;
}
