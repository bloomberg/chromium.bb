// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/palette/palette_utils.h"

#include "ash/common/ash_switches.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/system/chromeos/palette/palette_tray.h"
#include "ash/common/system/status_area_widget.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "base/command_line.h"
#include "ui/events/devices/input_device_manager.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/gfx/geometry/point.h"

namespace ash {
namespace palette_utils {

bool HasStylusInput() {
  // Allow the user to force-enable by passing a switch.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshForceEnablePalette)) {
    return true;
  }

  for (const ui::TouchscreenDevice& device :
       ui::InputDeviceManager::GetInstance()->GetTouchscreenDevices()) {
    if (device.is_stylus &&
        device.type == ui::InputDeviceType::INPUT_DEVICE_INTERNAL) {
      return true;
    }
  }

  return false;
}

bool IsPaletteEnabledOnEveryDisplay() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAshEnablePaletteOnAllDisplays);
}

bool PaletteContainsPointInScreen(const gfx::Point& point) {
  for (WmWindow* window : WmShell::Get()->GetAllRootWindows()) {
    PaletteTray* palette_tray =
        WmShelf::ForWindow(window)->GetStatusAreaWidget()->palette_tray();
    if (palette_tray && palette_tray->ContainsPointInScreen(point))
      return true;
  }

  return false;
}

}  // namespace palette_utils
}  // namespace ash
