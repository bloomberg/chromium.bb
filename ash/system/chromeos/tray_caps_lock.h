// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_TRAY_CAPS_LOCK_H_
#define ASH_SYSTEM_CHROMEOS_TRAY_CAPS_LOCK_H_

#include "ash/system/tray/tray_image_item.h"
#include "chromeos/ime/ime_keyboard.h"
#include "ui/events/event_handler.h"

namespace views {
class ImageView;
class View;
}

namespace ash {
class CapsLockDefaultView;

class TrayCapsLock : public TrayImageItem,
                     public chromeos::input_method::ImeKeyboard::Observer {
 public:
  explicit TrayCapsLock(SystemTray* system_tray);
  virtual ~TrayCapsLock();

 private:
  // Overriden from chromeos::input_method::ImeKeyboard::Observer:
  virtual void OnCapsLockChanged(bool enabled) override;

  // Overridden from TrayImageItem.
  virtual bool GetInitialVisibility() override;
  virtual views::View* CreateDefaultView(user::LoginStatus status) override;
  virtual views::View* CreateDetailedView(user::LoginStatus status) override;
  virtual void DestroyDefaultView() override;
  virtual void DestroyDetailedView() override;

  CapsLockDefaultView* default_;
  views::View* detailed_;

  bool caps_lock_enabled_;
  bool message_shown_;

  DISALLOW_COPY_AND_ASSIGN(TrayCapsLock);
};

}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_TRAY_CAPS_LOCK_H_
