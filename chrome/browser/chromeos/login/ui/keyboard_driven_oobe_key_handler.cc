// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/keyboard_driven_oobe_key_handler.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray.h"

namespace chromeos {

KeyboardDrivenOobeKeyHandler::KeyboardDrivenOobeKeyHandler() {
  ash::Shell::GetInstance()->AddPreTargetHandler(this);
}
KeyboardDrivenOobeKeyHandler::~KeyboardDrivenOobeKeyHandler() {
  ash::Shell::GetInstance()->RemovePreTargetHandler(this);
}

void KeyboardDrivenOobeKeyHandler::OnKeyEvent(ui::KeyEvent* event) {
  if (event->key_code() == ui::VKEY_F6) {
    ash::Shell::GetInstance()->GetPrimarySystemTray()->CloseSystemBubble();
    event->StopPropagation();
  }
}

}  // namespace chromeos
