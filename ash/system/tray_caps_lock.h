// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_CAPS_LOCK_H_
#define ASH_SYSTEM_TRAY_CAPS_LOCK_H_

#include "ash/system/tray/tray_image_item.h"
#include "base/macros.h"
#include "ui/base/ime/chromeos/ime_keyboard.h"
#include "ui/events/event_handler.h"

namespace views {
class View;
}

namespace ash {
class CapsLockDefaultView;

class TrayCapsLock : public TrayImageItem,
                     public chromeos::input_method::ImeKeyboard::Observer {
 public:
  explicit TrayCapsLock(SystemTray* system_tray);
  ~TrayCapsLock() override;

 private:
  // Overridden from chromeos::input_method::ImeKeyboard::Observer:
  void OnCapsLockChanged(bool enabled) override;
  void OnLayoutChanging(const std::string& layout_name) override {}

  // Overridden from TrayImageItem.
  bool GetInitialVisibility() override;
  views::View* CreateDefaultView(LoginStatus status) override;
  void OnDefaultViewDestroyed() override;

  CapsLockDefaultView* default_;

  bool caps_lock_enabled_;
  bool message_shown_;

  DISALLOW_COPY_AND_ASSIGN(TrayCapsLock);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_CAPS_LOCK_H_
