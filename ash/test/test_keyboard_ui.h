// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_TEST_KEYBOARD_UI_H_
#define ASH_TEST_TEST_KEYBOARD_UI_H_

#include "base/memory/scoped_ptr.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/keyboard/keyboard_ui.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

// Stub implementation of keyboard::KeyboardUI
class TestKeyboardUI : public keyboard::KeyboardUI {
 public:
  TestKeyboardUI();
  ~TestKeyboardUI() override;

  bool HasKeyboardWindow() const override;
  bool ShouldWindowOverscroll(aura::Window* window) const override;
  aura::Window* GetKeyboardWindow() override;

 private:
  // Overridden from keyboard::KeyboardUI:
  ui::InputMethod* GetInputMethod() override;
  void SetUpdateInputType(ui::TextInputType type) override;
  void ReloadKeyboardIfNeeded() override;
  void InitInsets(const gfx::Rect& keyboard_bounds) override;
  void ResetInsets() override;

  aura::test::TestWindowDelegate delegate_;
  scoped_ptr<aura::Window> keyboard_;
  DISALLOW_COPY_AND_ASSIGN(TestKeyboardUI);
};

}  // namespace ash

#endif  // ASH_TEST_TEST_KEYBOARD_UI_H_
