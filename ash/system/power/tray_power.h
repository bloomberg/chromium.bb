// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_TRAY_POWER_H_
#define ASH_SYSTEM_POWER_TRAY_POWER_H_

#include "ash/system/power/power_status_observer.h"
#include "ash/system/tray/system_tray_item.h"

class SkBitmap;

namespace gfx {
class ImageSkia;
}

namespace ash {
namespace internal {

namespace tray {
class PowerNotificationView;
class PowerTrayView;
}

enum IconSet {
  ICON_LIGHT,
  ICON_DARK
};

class TrayPower : public SystemTrayItem,
                  public PowerStatusObserver {
 public:
  TrayPower();
  virtual ~TrayPower();

  // Gets battery image based on |supply_status|. If |supply_status| is
  // uncertain about the power state, return |default_image| instead.
  static gfx::ImageSkia GetBatteryImage(const PowerSupplyStatus& supply_status,
                                        IconSet icon_set,
                                        const gfx::ImageSkia& default_image);

 private:
  enum NotificationState {
    NOTIFICATION_NONE,
    NOTIFICATION_LOW_POWER,
    NOTIFICATION_CRITICAL
  };

  // Overridden from SystemTrayItem.
  virtual views::View* CreateTrayView(user::LoginStatus status) OVERRIDE;
  virtual views::View* CreateDefaultView(user::LoginStatus status) OVERRIDE;
  virtual views::View* CreateNotificationView(
      user::LoginStatus status) OVERRIDE;
  virtual void DestroyTrayView() OVERRIDE;
  virtual void DestroyDefaultView() OVERRIDE;
  virtual void DestroyNotificationView() OVERRIDE;
  virtual void UpdateAfterLoginStatusChange(user::LoginStatus status) OVERRIDE;
  virtual void UpdateAfterShelfAlignmentChange(
      ShelfAlignment alignment) OVERRIDE;

  // Overridden from PowerStatusObserver.
  virtual void OnPowerStatusChanged(const PowerSupplyStatus& status) OVERRIDE;

  // Sets |notification_state_|. Returns true if a notification should be shown.
  bool UpdateNotificationState(const PowerSupplyStatus& status);

  tray::PowerTrayView* power_tray_;
  tray::PowerNotificationView* notification_view_;
  NotificationState notification_state_;

  DISALLOW_COPY_AND_ASSIGN(TrayPower);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_POWER_TRAY_POWER_H_
