// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/maximize_mode/scoped_disable_internal_mouse_and_keyboard_ozone.h"

#include <set>

#include "ash/shell.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"

namespace ash {

ScopedDisableInternalMouseAndKeyboardOzone::
    ScopedDisableInternalMouseAndKeyboardOzone() {
  ui::InputController* input_controller =
      ui::OzonePlatform::GetInstance()->GetInputController();
  if (input_controller->HasTouchpad() &&
      input_controller->IsInternalTouchpadEnabled()) {
    should_ignore_touch_pad_ = false;
    input_controller->SetInternalTouchpadEnabled(false);
    Shell::GetInstance()->cursor_manager()->HideCursor();
  }

  // Allow the acccessible keys present on the side of some devices to continue
  // working.
  std::vector<ui::DomCode> allowed_keys;
  allowed_keys.push_back(ui::DomCode::VOLUME_DOWN);
  allowed_keys.push_back(ui::DomCode::VOLUME_UP);
  allowed_keys.push_back(ui::DomCode::POWER);
  input_controller->SetInternalKeyboardFilter(true /* enable_filter */,
                                              allowed_keys);
}

ScopedDisableInternalMouseAndKeyboardOzone::
    ~ScopedDisableInternalMouseAndKeyboardOzone() {
  ui::InputController* input_controller =
      ui::OzonePlatform::GetInstance()->GetInputController();

  if (!should_ignore_touch_pad_) {
    input_controller->SetInternalTouchpadEnabled(true);
    Shell::GetInstance()->cursor_manager()->ShowCursor();
  }

  input_controller->SetInternalKeyboardFilter(false /* enable_filter */,
                                              std::vector<ui::DomCode>());
}

}  // namespace ash
