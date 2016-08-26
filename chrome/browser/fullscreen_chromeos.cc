// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fullscreen.h"

#include "ash/root_window_controller.h"
#include "chrome/browser/ui/ash/ash_util.h"

bool IsFullScreenMode() {
  if (chrome::IsRunningInMash()) {
    // TODO: http://crbug.com/640390.
    NOTIMPLEMENTED();
    return false;
  }
  // TODO(oshima): Fullscreen is per display state. Investigate
  // and fix if necessary.
  ash::RootWindowController* controller =
      ash::RootWindowController::ForTargetRootWindow();
  return controller && controller->GetWindowForFullscreenMode();
}
