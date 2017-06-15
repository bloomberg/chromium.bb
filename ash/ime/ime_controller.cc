// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ime/ime_controller.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray_notifier.h"

namespace ash {

ImeController::ImeController() = default;

ImeController::~ImeController() = default;

void ImeController::RefreshIme(
    const mojom::ImeInfo& current_ime,
    const std::vector<mojom::ImeInfo>& available_imes,
    const std::vector<mojom::ImeMenuItem>& menu_items) {
  current_ime_ = current_ime;
  available_imes_ = available_imes;
  current_ime_menu_items_ = menu_items;
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
