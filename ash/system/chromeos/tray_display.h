// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_TRAY_DISPLAY_H_
#define ASH_SYSTEM_CHROMEOS_TRAY_DISPLAY_H_

#include "ash/display/display_controller.h"
#include "ash/system/tray/system_tray_item.h"

namespace views {
class View;
}

namespace ash {
namespace internal {

enum TrayDisplayMode {
  TRAY_DISPLAY_SINGLE,
  TRAY_DISPLAY_EXTENDED,
  TRAY_DISPLAY_MIRRORED,
  TRAY_DISPLAY_DOCKED,
};

class DisplayView;
class DisplayNotificationView;

class TrayDisplay : public SystemTrayItem,
                    public DisplayController::Observer {
 public:
  explicit TrayDisplay(SystemTray* system_tray);
  virtual ~TrayDisplay();

 private:
  // Overridden from SystemTrayItem.
  virtual views::View* CreateDefaultView(user::LoginStatus status) OVERRIDE;
  virtual views::View* CreateNotificationView(
      user::LoginStatus status) OVERRIDE;
  virtual void DestroyDefaultView() OVERRIDE;
  virtual void DestroyNotificationView() OVERRIDE;
  virtual bool ShouldShowLauncher() const OVERRIDE;

  // Overridden from DisplayControllerObserver:
  virtual void OnDisplayConfigurationChanged() OVERRIDE;

  DisplayView* default_;
  DisplayNotificationView* notification_;
  TrayDisplayMode current_mode_;

  DISALLOW_COPY_AND_ASSIGN(TrayDisplay);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_TRAY_DISPLAY_H_
