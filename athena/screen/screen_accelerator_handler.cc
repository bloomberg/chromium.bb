// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/screen/screen_accelerator_handler.h"

#include "athena/input/public/accelerator_manager.h"
#include "athena/screen/public/screen_manager.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"

namespace athena {
namespace {

enum Command {
  CMD_ROTATE_SCREEN,
};

const AcceleratorData accelerator_data[] = {
    {TRIGGER_ON_PRESS, ui::VKEY_F3,
     ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN,
     CMD_ROTATE_SCREEN, AF_NONE},
};

void HandleRotateScreen() {
  ScreenManager* screen_manager = ScreenManager::Get();
  gfx::Display::Rotation current_rotation =
      gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().rotation();
  if (current_rotation == gfx::Display::ROTATE_0)
    screen_manager->SetRotation(gfx::Display::ROTATE_90);
  else if (current_rotation == gfx::Display::ROTATE_90)
    screen_manager->SetRotation(gfx::Display::ROTATE_180);
  else if (current_rotation == gfx::Display::ROTATE_180)
    screen_manager->SetRotation(gfx::Display::ROTATE_270);
  else if (current_rotation == gfx::Display::ROTATE_270)
    screen_manager->SetRotation(gfx::Display::ROTATE_0);
}

}  // namespace

ScreenAcceleratorHandler::ScreenAcceleratorHandler() {
  AcceleratorManager::Get()->RegisterAccelerators(
      accelerator_data, arraysize(accelerator_data), this);
}

ScreenAcceleratorHandler::~ScreenAcceleratorHandler() {
}

bool ScreenAcceleratorHandler::IsCommandEnabled(int command_id) const {
  return true;
}

bool ScreenAcceleratorHandler::OnAcceleratorFired(
    int command_id,
    const ui::Accelerator& accelerator) {
  switch (command_id) {
    case CMD_ROTATE_SCREEN:
      HandleRotateScreen();
      return true;
  }
  return false;
}

}  // namesapce athena
