// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_SCREEN_CAPTURE_SCREEN_CAPTURE_TRAY_ITEM_H_
#define ASH_SYSTEM_CHROMEOS_SCREEN_CAPTURE_SCREEN_CAPTURE_TRAY_ITEM_H_

#include "ash/system/chromeos/screen_security/screen_capture_observer.h"
#include "ash/system/chromeos/screen_security/screen_tray_item.h"

namespace views {
class View;
}

namespace ash {

class ASH_EXPORT ScreenCaptureTrayItem : public ScreenTrayItem,
                                         public ScreenCaptureObserver {
 public:
  explicit ScreenCaptureTrayItem(SystemTray* system_tray);
  virtual ~ScreenCaptureTrayItem();

 private:
  // Overridden from SystemTrayItem.
  virtual views::View* CreateTrayView(user::LoginStatus status) OVERRIDE;
  virtual views::View* CreateDefaultView(user::LoginStatus status) OVERRIDE;

  // Overridden from ScreenTrayItem.
  virtual void CreateOrUpdateNotification() OVERRIDE;
  virtual std::string GetNotificationId() OVERRIDE;

  // Overridden from ScreenCaptureObserver.
  virtual void OnScreenCaptureStart(
      const base::Closure& stop_callback,
      const base::string16& screen_capture_status) OVERRIDE;
  virtual void OnScreenCaptureStop() OVERRIDE;

  base::string16 screen_capture_status_;

  DISALLOW_COPY_AND_ASSIGN(ScreenCaptureTrayItem);
};

}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_SCREEN_CAPTURE_SCREEN_CAPTURE_TRAY_ITEM_H_
