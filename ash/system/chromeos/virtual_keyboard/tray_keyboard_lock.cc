// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/virtual_keyboard/tray_keyboard_lock.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/tray_item_more.h"
#include "ash/system/tray_accessibility.h"
#include "ash/virtual_keyboard_controller.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/display.h"
#include "ui/keyboard/keyboard_util.h"

namespace ash {

namespace tray {

// System tray item that handles toggling the state of the keyboard between
// enabled and disabled.
class KeyboardLockDefaultView : public TrayItemMore,
                                public VirtualKeyboardObserver,
                                public AccessibilityObserver {
 public:
  explicit KeyboardLockDefaultView(SystemTrayItem* owner, bool suppressed);
  virtual ~KeyboardLockDefaultView();

  // ActionableView
  bool PerformAction(const ui::Event& event) override;

  // VirtualKeyboardObserver
  void OnKeyboardSuppressionChanged(bool suppressed) override;

  // AccessibilityObserver:
  void OnAccessibilityModeChanged(
      ui::AccessibilityNotificationVisibility notify) override;

 private:
  void Update();

  // Whether the virtual keyboard is suppressed.
  bool suppressed_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardLockDefaultView);
};

KeyboardLockDefaultView::KeyboardLockDefaultView(SystemTrayItem* owner,
                                                 bool suppressed)
    : TrayItemMore(owner, false), suppressed_(suppressed) {
  Update();
  Shell::GetInstance()->system_tray_notifier()->AddVirtualKeyboardObserver(
      this);
  Shell::GetInstance()->system_tray_notifier()->AddAccessibilityObserver(this);
}

KeyboardLockDefaultView::~KeyboardLockDefaultView() {
  Shell::GetInstance()->system_tray_notifier()->RemoveVirtualKeyboardObserver(
      this);
  Shell::GetInstance()->system_tray_notifier()->RemoveAccessibilityObserver(
      this);
}

bool KeyboardLockDefaultView::PerformAction(const ui::Event& event) {
  Shell::GetInstance()
      ->virtual_keyboard_controller()
      ->ToggleIgnoreExternalKeyboard();
  return true;
}

void KeyboardLockDefaultView::Update() {
  base::string16 label;
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  if (keyboard::IsKeyboardEnabled()) {
    SetImage(
        bundle.GetImageSkiaNamed(IDR_AURA_UBER_TRAY_VIRTUAL_KEYBOARD_ENABLED));
    label = l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_KEYBOARD_ENABLED);
  } else {
    SetImage(
        bundle.GetImageSkiaNamed(IDR_AURA_UBER_TRAY_VIRTUAL_KEYBOARD_DISABLED));
    label = l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_KEYBOARD_DISABLED);
  }
  SetLabel(label);
  SetAccessibleName(label);
  SetVisible(suppressed_ &&
             !Shell::GetInstance()
                  ->accessibility_delegate()
                  ->IsVirtualKeyboardEnabled());
}

void KeyboardLockDefaultView::OnKeyboardSuppressionChanged(bool suppressed) {
  suppressed_ = suppressed;
  Update();
}

void KeyboardLockDefaultView::OnAccessibilityModeChanged(
    ui::AccessibilityNotificationVisibility notify) {
  Update();
}

}  // namespace tray

TrayKeyboardLock::TrayKeyboardLock(SystemTray* system_tray)
    : SystemTrayItem(system_tray), virtual_keyboard_suppressed_(false) {
  Shell::GetInstance()->system_tray_notifier()->AddVirtualKeyboardObserver(
      this);
}

TrayKeyboardLock::~TrayKeyboardLock() {
  Shell::GetInstance()->system_tray_notifier()->RemoveVirtualKeyboardObserver(
      this);
}

void TrayKeyboardLock::OnKeyboardSuppressionChanged(bool suppressed) {
  virtual_keyboard_suppressed_ = suppressed;
}

views::View* TrayKeyboardLock::CreateDefaultView(user::LoginStatus status) {
  return new tray::KeyboardLockDefaultView(this, virtual_keyboard_suppressed_);
}

}  // namespace ash
