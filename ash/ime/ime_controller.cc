// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ime/ime_controller.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray_notifier.h"

namespace ash {

ImeController::ImeController() = default;

ImeController::~ImeController() = default;

void ImeController::BindRequest(mojom::ImeControllerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void ImeController::SetClient(mojom::ImeControllerClientPtr client) {
  client_ = std::move(client);
}

bool ImeController::CanSwitchIme() const {
  NOTIMPLEMENTED();
  return true;
}

void ImeController::SwitchToNextIme() {
  NOTIMPLEMENTED();
}

void ImeController::SwitchToPreviousIme() {
  NOTIMPLEMENTED();
}

bool ImeController::CanSwitchImeWithAccelerator(
    const ui::Accelerator& accelerator) const {
  NOTIMPLEMENTED();
  return true;
}

void ImeController::SwitchImeWithAccelerator(
    const ui::Accelerator& accelerator) {
  NOTIMPLEMENTED();
}

// mojom::ImeController:
void ImeController::RefreshIme(mojom::ImeInfoPtr current_ime,
                               std::vector<mojom::ImeInfoPtr> available_imes,
                               std::vector<mojom::ImeMenuItemPtr> menu_items) {
  current_ime_ = *current_ime;

  available_imes_.clear();
  available_imes_.reserve(available_imes.size());
  for (const auto& ime : available_imes)
    available_imes_.push_back(*ime);

  current_ime_menu_items_.clear();
  current_ime_menu_items_.reserve(menu_items.size());
  for (const auto& item : menu_items)
    current_ime_menu_items_.push_back(*item);

  Shell::Get()->system_tray_notifier()->NotifyRefreshIME();
}

void ImeController::SetImesManagedByPolicy(bool managed) {
  managed_by_policy_ = managed;
  Shell::Get()->system_tray_notifier()->NotifyRefreshIME();
}

void ImeController::ShowImeMenuOnShelf(bool show) {
  Shell::Get()->system_tray_notifier()->NotifyRefreshIMEMenu(show);
}

}  // namespace ash
