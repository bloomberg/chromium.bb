// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/maximize_mode/scoped_disable_internal_mouse_and_keyboard_ozone.h"

#include <set>

#include "ash/shell.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/events/keycodes/dom3/dom_code.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"

namespace ash {

ScopedDisableInternalMouseAndKeyboardOzone::
    ScopedDisableInternalMouseAndKeyboardOzone() {
  ui::InputController* input_controller =
      ui::OzonePlatform::GetInstance()->GetInputController();
  if (input_controller->HasTouchpad()) {
    input_controller->DisableInternalTouchpad();
    aura::client::GetCursorClient(Shell::GetInstance()->GetPrimaryRootWindow())
        ->HideCursor();
  }

  // Allow the acccessible keys present on the side of some devices to continue
  // working.
  scoped_ptr<std::set<ui::DomCode>> excepted_keys(new std::set<ui::DomCode>);
  excepted_keys->insert(ui::DomCode::VOLUME_DOWN);
  excepted_keys->insert(ui::DomCode::VOLUME_UP);
  excepted_keys->insert(ui::DomCode::POWER);
  input_controller->DisableInternalKeyboardExceptKeys(excepted_keys.Pass());
}

ScopedDisableInternalMouseAndKeyboardOzone::
    ~ScopedDisableInternalMouseAndKeyboardOzone() {
  ui::InputController* input_controller =
      ui::OzonePlatform::GetInstance()->GetInputController();
  input_controller->EnableInternalTouchpad();
  input_controller->EnableInternalKeyboard();
}

}  // namespace ash
