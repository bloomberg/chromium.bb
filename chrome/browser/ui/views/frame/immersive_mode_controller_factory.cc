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
  // Immersive fullscreen is on by default.
  return !command->HasSwitch(ash::switches::kAshDisableImmersiveFullscreen);
#endif
  return false;
}

// Implemented here so all the code dealing with flags lives in one place.
void EnableImmersiveFullscreenForTest() {
  // Immersive fullscreen is on by default. If we turn it off, this function
  // will need to add kAshEnableImmersiveFullscreen to the command line.
}

ImmersiveModeController* CreateImmersiveModeController() {
#if defined(OS_CHROMEOS)
  return new ImmersiveModeControllerAsh();
#else
  return new ImmersiveModeControllerStub();
#endif
}

}  // namespace chrome
