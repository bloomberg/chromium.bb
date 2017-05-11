// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/oobe_display_chooser.h"

#include "ash/display/display_configuration_controller.h"
#include "ash/shell.h"
#include "ui/display/display.h"
#include "ui/display/display_layout.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"

namespace chromeos {

namespace {

bool TouchSupportAvailable(const display::Display& display) {
  return display.touch_support() ==
         display::Display::TouchSupport::TOUCH_SUPPORT_AVAILABLE;
}

}  // namespace

OobeDisplayChooser::OobeDisplayChooser() {}

OobeDisplayChooser::~OobeDisplayChooser() {}

void OobeDisplayChooser::TryToPlaceUiOnTouchDisplay() {
  display::Display primary_display =
      display::Screen::GetScreen()->GetPrimaryDisplay();

  if (primary_display.is_valid() && !TouchSupportAvailable(primary_display))
    MoveToTouchDisplay();
}

void OobeDisplayChooser::MoveToTouchDisplay() {
  const display::Displays& displays =
      ash::Shell::Get()->display_manager()->active_only_display_list();

  if (displays.size() <= 1)
    return;

  for (const display::Display& display : displays) {
    if (TouchSupportAvailable(display)) {
      ash::Shell::Get()
          ->display_configuration_controller()
          ->SetPrimaryDisplayId(display.id());
      break;
    }
  }
}

}  // namespace chromeos
