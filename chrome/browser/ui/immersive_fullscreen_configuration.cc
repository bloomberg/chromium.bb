// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/immersive_fullscreen_configuration.h"

#if defined(OS_CHROMEOS)
#include "ash/ash_switches.h"
#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#endif  // defined(OS_CHROMEOS)

// static
bool ImmersiveFullscreenConfiguration::UseImmersiveFullscreen() {
#if defined(OS_CHROMEOS)
  CommandLine* command = CommandLine::ForCurrentProcess();
  // Kiosk mode needs the whole screen.
  if (command->HasSwitch(switches::kKioskMode))
    return false;
  // Immersive fullscreen is on by default. If you change the default you must
  // change the enable function below and BrowserTest FullscreenBookmarkBar
  // (which cannot depend on this function due to DEPS).
  return !command->HasSwitch(ash::switches::kAshDisableImmersiveFullscreen);
#endif
  return false;
}

// static
// Implemented here so all the code dealing with flags lives in one place.
void ImmersiveFullscreenConfiguration::EnableImmersiveFullscreenForTest() {
  // Immersive fullscreen is on by default. If we turn it off, this function
  // will need to add kAshEnableImmersiveFullscreen to the command line.
}

int ImmersiveFullscreenConfiguration::immersive_mode_reveal_delay_ms_ = 200;
int
ImmersiveFullscreenConfiguration::immersive_mode_reveal_x_threshold_pixels_ = 3;
