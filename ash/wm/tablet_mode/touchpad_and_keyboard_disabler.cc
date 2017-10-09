// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/touchpad_and_keyboard_disabler.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "services/ui/public/cpp/input_devices/input_device_controller_client.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace ash {

namespace {

void OnReenableTouchpadDone(bool succeeded) {}

ui::InputDeviceControllerClient* GetInputDeviceControllerClient() {
  return Shell::Get()->shell_delegate()->GetInputDeviceControllerClient();
}

class DefaultDelegateImpl : public TouchpadAndKeyboardDisabler::Delegate {
 public:
  DefaultDelegateImpl() = default;
  ~DefaultDelegateImpl() override {
    std::vector<ui::DomCode> allowed_keys;
    const bool enable_filter = false;
    GetInputDeviceControllerClient()->SetInternalKeyboardFilter(enable_filter,
                                                                allowed_keys);
  }

  // TouchpadAndKeyboardDisabler::Delegate:
  void Disable(DisableResultClosure callback) override {
    GetInputDeviceControllerClient()->SetInternalTouchpadEnabled(
        false, std::move(callback));

    // Allow the acccessible keys present on the side of some devices to
    // continue working.
    std::vector<ui::DomCode> allowed_keys;
    allowed_keys.push_back(ui::DomCode::VOLUME_DOWN);
    allowed_keys.push_back(ui::DomCode::VOLUME_UP);
    allowed_keys.push_back(ui::DomCode::POWER);
    const bool enable_filter = true;
    GetInputDeviceControllerClient()->SetInternalKeyboardFilter(enable_filter,
                                                                allowed_keys);
  }
  void HideCursor() override {
    if (Shell::Get()->cursor_manager())
      Shell::Get()->cursor_manager()->HideCursor();
  }
  void Enable() override {
    GetInputDeviceControllerClient()->SetInternalTouchpadEnabled(
        true, base::BindOnce(&OnReenableTouchpadDone));
    if (Shell::Get()->cursor_manager())
      Shell::Get()->cursor_manager()->ShowCursor();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultDelegateImpl);
};

}  // namespace

TouchpadAndKeyboardDisabler::TouchpadAndKeyboardDisabler(
    std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)), weak_ptr_factory_(this) {
  if (!delegate_)
    delegate_ = std::make_unique<DefaultDelegateImpl>();
  Shell::Get()->AddShellObserver(this);
  delegate_->Disable(base::BindOnce(&TouchpadAndKeyboardDisabler::OnDisableAck,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void TouchpadAndKeyboardDisabler::Destroy() {
  DCHECK(!delete_on_ack_);
  delete_on_ack_ = true;
  if (got_ack_)
    delete this;
}

TouchpadAndKeyboardDisabler::~TouchpadAndKeyboardDisabler() {
  Shell::Get()->RemoveShellObserver(this);
  if (did_disable_)
    delegate_->Enable();
}

void TouchpadAndKeyboardDisabler::OnDisableAck(bool succeeded) {
  got_ack_ = true;
  did_disable_ = succeeded;
  if (delete_on_ack_) {
    delete this;
    return;
  }
  if (!succeeded)
    return;
  delegate_->HideCursor();
}

void TouchpadAndKeyboardDisabler::OnShellDestroyed() {
  // ScopedDisableInternalMouseAndKeyboardOzone should have been deleted.
  DCHECK(delete_on_ack_);
  delete this;
}

}  // namespace ash
