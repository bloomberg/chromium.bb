// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/keyboard_ui.h"

#include "ash/accessibility/accessibility_delegate.h"
#include "ash/keyboard/keyboard_ui_observer.h"
#include "ash/shell.h"
#include "ash/system/accessibility_observer.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray_accessibility.h"
#include "base/memory/ptr_util.h"
#include "ui/keyboard/keyboard_controller.h"

namespace ash {

class KeyboardUIImpl : public KeyboardUI, public AccessibilityObserver {
 public:
  KeyboardUIImpl() : enabled_(false) {
    Shell::Get()->system_tray_notifier()->AddAccessibilityObserver(this);
  }

  ~KeyboardUIImpl() override {
    if (Shell::HasInstance() && Shell::Get()->system_tray_notifier())
      Shell::Get()->system_tray_notifier()->RemoveAccessibilityObserver(this);
  }

  void ShowInDisplay(const int64_t display_id) override {
    keyboard::KeyboardController* controller =
        keyboard::KeyboardController::GetInstance();
    // Controller may not exist if keyboard has been disabled. crbug.com/749989
    if (!controller)
      return;
    controller->ShowKeyboardInDisplay(display_id);
  }
  void Hide() override {
    // Do nothing as this is called from ash::Shell, which also calls through
    // to the appropriate keyboard functions.
  }
  bool IsEnabled() override {
    return Shell::Get()->accessibility_delegate()->IsVirtualKeyboardEnabled();
  }

  // AccessibilityObserver:
  void OnAccessibilityStatusChanged(
      AccessibilityNotificationVisibility notify) override {
    bool enabled = IsEnabled();
    if (enabled_ == enabled)
      return;

    enabled_ = enabled;
    for (auto& observer : *observers())
      observer.OnKeyboardEnabledStateChanged(enabled);
  }

 private:
  bool enabled_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardUIImpl);
};

KeyboardUI::~KeyboardUI() {}

// static
std::unique_ptr<KeyboardUI> KeyboardUI::Create() {
  return base::WrapUnique(new KeyboardUIImpl);
}

void KeyboardUI::AddObserver(KeyboardUIObserver* observer) {
  observers_.AddObserver(observer);
}

void KeyboardUI::RemoveObserver(KeyboardUIObserver* observer) {
  observers_.RemoveObserver(observer);
}

KeyboardUI::KeyboardUI() {}

}  // namespace ash
