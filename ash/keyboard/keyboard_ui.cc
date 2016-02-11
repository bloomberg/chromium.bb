// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/keyboard_ui.h"

#include "ash/accessibility_delegate.h"
#include "ash/keyboard/keyboard_ui_observer.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray_accessibility.h"
#include "ui/keyboard/keyboard_controller.h"

namespace ash {

class KeyboardUIImpl : public KeyboardUI, public AccessibilityObserver {
 public:
  KeyboardUIImpl() {
    // The Shell may not exist in some unit tests.
    Shell::GetInstance()->system_tray_notifier()->AddAccessibilityObserver(
        this);
  }

  ~KeyboardUIImpl() override {}

  // KeyboardUI:
  void Show() override {
    keyboard::KeyboardController::GetInstance()->ShowKeyboard(true);
  }
  void Hide() override {
    // Do nothing as this is called from ash::Shell, which also calls through
    // to the appropriate keyboard functions.
  }
  bool IsEnabled() override {
    return Shell::GetInstance()
        ->accessibility_delegate()
        ->IsVirtualKeyboardEnabled();
  }

  // AccessibilityObserver:
  void OnAccessibilityModeChanged(
      ui::AccessibilityNotificationVisibility notify) override {
    FOR_EACH_OBSERVER(KeyboardUIObserver, *observers(),
                      OnKeyboardEnabledStateChanged(IsEnabled()));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(KeyboardUIImpl);
};

KeyboardUI::~KeyboardUI() {}

// static
scoped_ptr<KeyboardUI> KeyboardUI::Create() {
  return make_scoped_ptr(new KeyboardUIImpl);
}

void KeyboardUI::AddObserver(KeyboardUIObserver* observer) {
  observers_.AddObserver(observer);
}

void KeyboardUI::RemoveObserver(KeyboardUIObserver* observer) {
  observers_.RemoveObserver(observer);
}

KeyboardUI::KeyboardUI() {}

}  // namespace ash
