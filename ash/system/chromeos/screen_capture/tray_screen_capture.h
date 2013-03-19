// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_SCREEN_CAPTURE_TRAY_SCREEN_CAPTURE_H_
#define ASH_SYSTEM_CHROMEOS_SCREEN_CAPTURE_TRAY_SCREEN_CAPTURE_H_

#include "ash/system/tray/system_tray_item.h"
#include "chromeos/screen_capture/screen_capture_observer.h"

namespace views {
class View;
}

namespace ash {
namespace internal {

namespace tray{
class ScreenCaptureTrayView;
class ScreenCaptureStatusView;
class ScreenCaptureNotificationView;
}  // namespace tray

class TrayScreenCapture : public SystemTrayItem,
                          public chromeos::ScreenCaptureObserver {
 public:
  explicit TrayScreenCapture(SystemTray* system_tray);
  virtual ~TrayScreenCapture();

  void Update();
  bool IsScreenCaptureOn();
  void SetScreenCaptureOn(bool screen_capture_on);

 private:
  // Overridden from SystemTrayItem.
  virtual views::View* CreateTrayView(user::LoginStatus status) OVERRIDE;
  virtual views::View* CreateDefaultView(user::LoginStatus status) OVERRIDE;
  virtual views::View* CreateNotificationView(
      user::LoginStatus status) OVERRIDE;
  virtual void DestroyTrayView() OVERRIDE;
  virtual void DestroyDefaultView() OVERRIDE;
  virtual void DestroyNotificationView() OVERRIDE;

  // Overridden from chromeos::ScreenCaptureObserver.
  virtual void OnScreenCaptureStatusChanged() OVERRIDE;

  tray::ScreenCaptureTrayView* tray_;
  tray::ScreenCaptureStatusView* default_;
  tray::ScreenCaptureNotificationView* notification_;

  DISALLOW_COPY_AND_ASSIGN(TrayScreenCapture);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_SCREEN_CAPTURE_TRAY_SCREEN_CAPTURE_H_
