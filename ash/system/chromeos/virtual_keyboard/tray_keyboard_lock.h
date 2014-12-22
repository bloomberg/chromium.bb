// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_VIRTUAL_KEYBOARD_TRAY_KEYBOARD_LOCK_H
#define ASH_SYSTEM_CHROMEOS_VIRTUAL_KEYBOARD_TRAY_KEYBOARD_LOCK_H

#include "ash/shell_observer.h"
#include "ash/system/chromeos/virtual_keyboard/virtual_keyboard_observer.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_image_item.h"
#include "ash/system/tray_accessibility.h"
#include "ash/wm/maximize_mode/maximize_mode_controller.h"

namespace ash {

// TrayKeyboardLock is a provider of the default view for the SystemTray. This
// view indicates the current state of the virtual keyboard when an external
// keyboard is plugged in. The default view can be interacted with, it toggles
// the state of the keyboard lock.
class ASH_EXPORT TrayKeyboardLock : public SystemTrayItem,
                                    public VirtualKeyboardObserver {
 public:
  explicit TrayKeyboardLock(SystemTray* system_tray);
  virtual ~TrayKeyboardLock();

  // VirtualKeyboardObserver
  virtual void OnKeyboardSuppressionChanged(bool suppressed) override;

  // SystemTrayItem:
  virtual views::View* CreateDefaultView(user::LoginStatus status) override;

 private:
  friend class TrayKeyboardLockTest;

  // True if the on-screen keyboard is suppressed.
  bool virtual_keyboard_suppressed_;

  DISALLOW_COPY_AND_ASSIGN(TrayKeyboardLock);
};

}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_VIRTUAL_KEYBOARD_TRAY_KEYBOARD_LOCK_H
