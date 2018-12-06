// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/events/switch_access_event_handler.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/shell.h"

namespace ash {

namespace {

bool IsSwitchAccessEnabled() {
  return Shell::Get()->accessibility_controller()->switch_access_enabled();
}

void CancelEvent(ui::Event* event) {
  DCHECK(event);
  if (event->cancelable()) {
    event->SetHandled();
    event->StopPropagation();
  }
}

}  // namespace

SwitchAccessEventHandler::SwitchAccessEventHandler(
    mojom::SwitchAccessEventHandlerDelegatePtr delegate_ptr)
    : delegate_ptr_(std::move(delegate_ptr)), ignore_virtual_key_events_(true) {
  DCHECK(delegate_ptr_.is_bound());
  Shell::Get()->AddPreTargetHandler(this,
                                    ui::EventTarget::Priority::kAccessibility);
}

SwitchAccessEventHandler::~SwitchAccessEventHandler() {
  Shell::Get()->RemovePreTargetHandler(this);
}

void SwitchAccessEventHandler::FlushMojoForTest() {
  delegate_ptr_.FlushForTesting();
}

void SwitchAccessEventHandler::OnKeyEvent(ui::KeyEvent* event) {
  DCHECK(IsSwitchAccessEnabled());
  DCHECK(event);

  // Ignore virtual key events so users can type with the onscreen keyboard.
  if (ignore_virtual_key_events_ && !event->HasNativeEvent())
    return;

  ui::KeyboardCode key_code = event->key_code();
  if (keys_to_capture_.find(key_code) != keys_to_capture_.end()) {
    CancelEvent(event);
    delegate_ptr_->DispatchKeyEvent(ui::Event::Clone(*event));
  }
}

}  // namespace ash
