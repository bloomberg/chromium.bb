// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_SCREEN_CAPTURE_SCREEN_CAPTURE_TRAY_ITEM_H_
#define ASH_SYSTEM_CHROMEOS_SCREEN_CAPTURE_SCREEN_CAPTURE_TRAY_ITEM_H_

#include "ash/shell_observer.h"
#include "ash/system/chromeos/screen_security/screen_capture_observer.h"
#include "ash/system/chromeos/screen_security/screen_tray_item.h"

namespace views {
class View;
}

namespace ash {

class ASH_EXPORT ScreenCaptureTrayItem : public ScreenTrayItem,
                                         public ScreenCaptureObserver,
                                         public ShellObserver {
 public:
  explicit ScreenCaptureTrayItem(SystemTray* system_tray);
  ~ScreenCaptureTrayItem() override;

 private:
  // Overridden from SystemTrayItem.
  views::View* CreateTrayView(user::LoginStatus status) override;
  views::View* CreateDefaultView(user::LoginStatus status) override;

  // Overridden from ScreenTrayItem.
  void CreateOrUpdateNotification() override;
  std::string GetNotificationId() override;

  // Overridden from ScreenCaptureObserver.
  void OnScreenCaptureStart(
      const base::Closure& stop_callback,
      const base::string16& screen_capture_status) override;
  void OnScreenCaptureStop() override;

  // Overridden from ShellObserver.
  void OnCastingSessionStartedOrStopped(bool started) override;

  base::string16 screen_capture_status_;
  bool is_casting_ = false;

  DISALLOW_COPY_AND_ASSIGN(ScreenCaptureTrayItem);
};

}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_SCREEN_CAPTURE_SCREEN_CAPTURE_TRAY_ITEM_H_
