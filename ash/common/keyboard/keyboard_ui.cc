// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/keyboard/keyboard_ui.h"

#include "ash/common/accessibility_delegate.h"
#include "ash/common/keyboard/keyboard_ui_observer.h"
#include "ash/common/system/accessibility_observer.h"
#include "ash/common/system/tray/system_tray_notifier.h"
#include "ash/common/system/tray_accessibility.h"
#include "ash/common/wm_shell.h"
#include "base/memory/ptr_util.h"
#include "ui/keyboard/keyboard_controller.h"

namespace ash {

class KeyboardUIImpl : public KeyboardUI, public AccessibilityObserver {
 public:
  KeyboardUIImpl() : enabled_(false) {
    WmShell::Get()->system_tray_notifier()->AddAccessibilityObserver(this);
  }

  ~KeyboardUIImpl() override {
    if (WmShell::HasInstance() && WmShell::Get()->system_tray_notifier())
      WmShell::Get()->system_tray_notifier()->RemoveAccessibilityObserver(this);
  }

  // KeyboardUI:
  void Show() override {
    keyboard::KeyboardController::GetInstance()->ShowKeyboard(true);
  }
  void ShowInDisplay(const int64_t display_id) override {
    keyboard::KeyboardController::GetInstance()->ShowKeyboardInDisplay(
        display_id);
  }
  void Hide() override {
    // Do nothing as this is called from ash::Shell, which also calls through
    // to the appropriate keyboard functions.
  }
  bool IsEnabled() override {
    return WmShell::Get()->accessibility_delegate()->IsVirtualKeyboardEnabled();
  }

  // AccessibilityObserver:
  void OnAccessibilityModeChanged(
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
