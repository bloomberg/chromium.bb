// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_IME_IME_CONTROLLER_H_
#define ASH_IME_IME_CONTROLLER_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/public/interfaces/ime_info.mojom.h"
#include "base/macros.h"

namespace ash {

// Connects ash IME users (e.g. the system tray) to the IME implementation,
// which might live in Chrome browser or in a separate mojo service.
// TODO(jamescook): Convert to use mojo IME interface to Chrome browser.
class ASH_EXPORT ImeController {
 public:
  ImeController();
  ~ImeController();

  const mojom::ImeInfo& current_ime() const { return current_ime_; }

  const std::vector<mojom::ImeInfo>& available_imes() const {
    return available_imes_;
  }

  bool managed_by_policy() const { return managed_by_policy_; }

  const std::vector<mojom::ImeMenuItem>& current_ime_menu_items() const {
    return current_ime_menu_items_;
  }

  // Updates the cached IME information and refreshes the UI.
  // TODO(jamescook): This should take an ID for current_ime.
  void RefreshIme(const mojom::ImeInfo& current_ime,
                  const std::vector<mojom::ImeInfo>& available_imes,
                  const std::vector<mojom::ImeMenuItem>& menu_items);

  void SetImesManagedByPolicy(bool managed);

  // Shows the IME menu on the shelf instead of inside the system tray menu.
  void ShowImeMenuOnShelf(bool show);

 private:
  mojom::ImeInfo current_ime_;

  // "Available" IMEs are both installed and enabled by the user in settings.
  std::vector<mojom::ImeInfo> available_imes_;

  // True if the available IMEs are currently managed by enterprise policy.
  // For example, can occur at the login screen with device-level policy.
  bool managed_by_policy_ = false;

  // Additional menu items for properties of the currently selected IME.
  std::vector<mojom::ImeMenuItem> current_ime_menu_items_;

  DISALLOW_COPY_AND_ASSIGN(ImeController);
};

}  // namespace ash

#endif  // ASH_IME_IME_CONTROLLER_H_
