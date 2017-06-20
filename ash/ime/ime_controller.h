// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_IME_IME_CONTROLLER_H_
#define ASH_IME_IME_CONTROLLER_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/public/interfaces/ime_controller.mojom.h"
#include "ash/public/interfaces/ime_info.mojom.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace ui {
class Accelerator;
}

namespace ash {

// Connects ash IME users (e.g. the system tray) to the IME implementation,
// which might live in Chrome browser or in a separate mojo service.
class ASH_EXPORT ImeController
    : public NON_EXPORTED_BASE(mojom::ImeController) {
 public:
  ImeController();
  ~ImeController() override;

  const mojom::ImeInfo& current_ime() const { return current_ime_; }

  const std::vector<mojom::ImeInfo>& available_imes() const {
    return available_imes_;
  }

  bool managed_by_policy() const { return managed_by_policy_; }

  const std::vector<mojom::ImeMenuItem>& current_ime_menu_items() const {
    return current_ime_menu_items_;
  }

  // Binds the mojo interface to this object.
  void BindRequest(mojom::ImeControllerRequest request);

  // TODO(jamescook): Implement these. http://crbug.com/724305
  bool CanSwitchIme() const;
  void SwitchToNextIme();
  void SwitchToPreviousIme();
  bool CanSwitchImeWithAccelerator(const ui::Accelerator& accelerator) const;
  void SwitchImeWithAccelerator(const ui::Accelerator& accelerator);

  // mojom::ImeController:
  void SetClient(mojom::ImeControllerClientPtr client) override;
  void RefreshIme(mojom::ImeInfoPtr current_ime,
                  std::vector<mojom::ImeInfoPtr> available_imes,
                  std::vector<mojom::ImeMenuItemPtr> menu_items) override;
  void SetImesManagedByPolicy(bool managed) override;
  void ShowImeMenuOnShelf(bool show) override;

 private:
  // Bindings for users of the mojo interface.
  mojo::BindingSet<mojom::ImeController> bindings_;

  // Client interface back to IME code in chrome.
  mojom::ImeControllerClientPtr client_;

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
