// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"

#include "chrome/browser/ui/views/frame/immersive_mode_controller_stub.h"

#if defined(OS_CHROMEOS)
#include "ash/ash_switches.h"
#include "base/command_line.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller_ash.h"
#include "chrome/common/chrome_switches.h"
#endif  // defined(OS_CHROMEOS)

namespace chrome {

bool UseImmersiveFullscreen() {
#if defined(OS_CHROMEOS)
  CommandLine* command = CommandLine::ForCurrentProcess();
  // Kiosk mode needs the whole screen.
  if (command->HasSwitch(switches::kKioskMode))
    return false;
  // Immersive fullscreen is off by default. If you change the default you must
  // change the enable function below and BrowserTest FullscreenBookmarkBar
  // (which cannot depend on this function due to DEPS).
  return command->HasSwitch(ash::switches::kAshEnableImmersiveFullscreen);
#endif
  return false;
}

// Implemented here so all the code dealing with flags lives in one place.
void EnableImmersiveFullscreenForTest() {
#if defined(OS_CHROMEOS)
  // Immersive fullscreen is off by default.
  CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kAshEnableImmersiveFullscreen);
#endif
}

ImmersiveModeController* CreateImmersiveModeController() {
#if defined(OS_CHROMEOS)
  return new ImmersiveModeControllerAsh();
#else
  return new ImmersiveModeControllerStub();
#endif
}

}  // namespace chrome
