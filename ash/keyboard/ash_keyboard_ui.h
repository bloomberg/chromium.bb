// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KEYBOARD_ASH_KEYBOARD_UI_H_
#define ASH_KEYBOARD_ASH_KEYBOARD_UI_H_

#include <memory>

#include "base/macros.h"
#include "base/unguessable_token.h"
#include "ui/aura/window_observer.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_ui.h"

namespace aura {
class Window;
}

namespace ash {

class AshKeyboardController;

// Ash implementation of keyboard::KeyboardUI for Mash. This class owns a Widget
// hosting the keyboard and serves as a delegate for
// keybaord::KeyboardController. This class calls into AshKeyboardController to
// send mojo events to Chrome, and receives calls from Chrome through
// AshKeyboardController.
//
// The owned widget (AshKeyboardView) embeds the contents rendered in Chrome
// using ws::ServerRemoteViewHost and provides a token through
// keyboard.mojo.KeyboardContentsLoaded (handled by AshKeyboardController and
// passed to AshKeyboardUI through KeyboardContentsLoaded).

class AshKeyboardUI : public keyboard::KeyboardUI, public aura::WindowObserver {
 public:
  explicit AshKeyboardUI(AshKeyboardController* ash_keyboard_controller);
  ~AshKeyboardUI() override;

  // keyboard::KeyboardUI:
  aura::Window* LoadKeyboardWindow(LoadCallback callback) override;
  aura::Window* GetKeyboardWindow() const override;
  ui::InputMethod* GetInputMethod() override;
  void ReloadKeyboardIfNeeded() override;
  void KeyboardContentsLoaded(const base::UnguessableToken& token,
                              const gfx::Size& size) override;

  // aura::WindowObserver:
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowDestroyed(aura::Window* window) override;
  void OnWindowParentChanged(aura::Window* window,
                             aura::Window* parent) override;

 private:
  class AshKeyboardView;

  // Called once KeyboardContentsLoaded is called from the client and the
  // keyboard controller calls LoadKeyboardWindow. Both calls are asynchronous
  // so the order is indeterminate. Embeds the contents window and calls
  // |load_callback_| if set.
  void EmbedContents();

  // Sets the shadow elevation depending on the keyboard container type.
  void SetShadowElevation();

  AshKeyboardController* ash_keyboard_controller_;  // unowned
  keyboard::KeyboardUI::LoadCallback load_callback_;
  base::UnguessableToken contents_window_token_;
  gfx::Size contents_window_size_;
  std::unique_ptr<AshKeyboardView> ash_keyboard_view_;

  DISALLOW_COPY_AND_ASSIGN(AshKeyboardUI);
};

}  // namespace ash

#endif  // ASH_KEYBOARD_ASH_KEYBOARD_UI_H_
