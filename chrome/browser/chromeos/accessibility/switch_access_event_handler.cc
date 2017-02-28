// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/switch_access_event_handler.h"

#include "ash/shell.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/event_handler_common.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/browser/extension_host.h"
#include "ui/events/event.h"

namespace chromeos {

SwitchAccessEventHandler::SwitchAccessEventHandler() {
  if (ash::Shell::HasInstance())
    ash::Shell::GetInstance()->AddPreTargetHandler(this);
}

SwitchAccessEventHandler::~SwitchAccessEventHandler() {
  if (ash::Shell::HasInstance())
    ash::Shell::GetInstance()->RemovePreTargetHandler(this);
}

void SwitchAccessEventHandler::OnKeyEvent(ui::KeyEvent* event) {
  DCHECK(event);

  ui::KeyboardCode key_code = event->key_code();
  if (key_code == ui::VKEY_1 || key_code == ui::VKEY_2 ||
      key_code == ui::VKEY_3 || key_code == ui::VKEY_4 ||
      key_code == ui::VKEY_5) {
    CancelEvent(event);
    LOG(ERROR) << "Dispatching key " << key_code - ui::VKEY_0
               << " to switch access";
    DispatchKeyEventToSwitchAccess(*event);
  }
}

void SwitchAccessEventHandler::CancelEvent(ui::Event* event) {
  DCHECK(event);
  if (event->cancelable()) {
    event->SetHandled();
    event->StopPropagation();
  }
}

void SwitchAccessEventHandler::DispatchKeyEventToSwitchAccess(
    const ui::KeyEvent& event) {
  extensions::ExtensionHost* host =
      GetAccessibilityExtensionHost(extension_misc::kSwitchAccessExtensionId);
  ForwardKeyToExtension(event, host);
}

}  // namespace chromeos
