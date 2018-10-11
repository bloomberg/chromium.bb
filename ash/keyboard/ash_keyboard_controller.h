// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KEYBOARD_ASH_KEYBOARD_CONTROLLER_H_
#define ASH_KEYBOARD_ASH_KEYBOARD_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/interfaces/keyboard_controller.mojom.h"
#include "ash/session/session_observer.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "ui/keyboard/keyboard_controller_observer.h"

namespace gfx {
class Rect;
}

namespace keyboard {
class KeyboardController;
}

namespace ash {

class SessionController;

// Contains and observes a keyboard::KeyboardController instance. Ash specific
// behavior, including implementing the mojo interface, is implemented in this
// class. TODO(stevenjb): Consider re-factoring keyboard::KeyboardController so
// that this can inherit from that class instead.
class ASH_EXPORT AshKeyboardController
    : public mojom::KeyboardController,
      public keyboard::KeyboardControllerObserver,
      public SessionObserver {
 public:
  // |session_controller| is expected to outlive AshKeyboardController.
  explicit AshKeyboardController(SessionController* session_controller);
  ~AshKeyboardController() override;

  void BindRequest(mojom::KeyboardControllerRequest request);

  // Enables the keyboard controller if enabling has been requested. If already
  // enabled, the keyboard is disabled and re-enabled.
  void EnableKeyboard();

  // Disables the keyboard.
  void DisableKeyboard();

  // mojom::KeyboardController:
  void AddObserver(
      mojom::KeyboardControllerObserverAssociatedPtrInfo observer) override;
  void GetKeyboardConfig(GetKeyboardConfigCallback callback) override;
  void SetKeyboardConfig(
      keyboard::mojom::KeyboardConfigPtr keyboard_config) override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  keyboard::KeyboardController* keyboard_controller() {
    return keyboard_controller_.get();
  }

 private:
  // Ensures that the keyboard controller is activated for the primary window.
  void ActivateKeyboard();

  // keyboard::KeyboardControllerObserver
  void OnKeyboardConfigChanged() override;
  void OnKeyboardVisibilityStateChanged(bool is_visible) override;
  void OnKeyboardVisibleBoundsChanged(const gfx::Rect& bounds) override;
  void OnKeyboardDisabled() override;

  SessionController* session_controller_;  // unowned
  std::unique_ptr<keyboard::KeyboardController> keyboard_controller_;
  mojo::BindingSet<mojom::KeyboardController> bindings_;
  mojo::AssociatedInterfacePtrSet<mojom::KeyboardControllerObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(AshKeyboardController);
};

}  // namespace ash

#endif  // ASH_KEYBOARD_ASH_KEYBOARD_CONTROLLER_H_
