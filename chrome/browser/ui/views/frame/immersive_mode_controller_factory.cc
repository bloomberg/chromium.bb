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
  // Kiosk mode needs the whole screen.
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  return !command_line->HasSwitch(switches::kKioskMode) &&
      command_line->HasSwitch(ash::switches::kAshImmersiveFullscreen);
#endif
  return false;
}

ImmersiveModeController* CreateImmersiveModeController() {
#if defined(OS_CHROMEOS)
  return new ImmersiveModeControllerAsh();
#else
  return new ImmersiveModeControllerStub();
#endif
}

}  // namespace chrome
