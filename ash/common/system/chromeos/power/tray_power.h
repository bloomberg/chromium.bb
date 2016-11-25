// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_CHROMEOS_POWER_TRAY_POWER_H_
#define ASH_COMMON_SYSTEM_CHROMEOS_POWER_TRAY_POWER_H_

#include <memory>

#include "ash/common/system/chromeos/power/power_status.h"
#include "ash/common/system/tray/system_tray_item.h"
#include "base/macros.h"

namespace message_center {
class MessageCenter;
}

namespace ash {

class BatteryNotification;
class DualRoleNotification;

namespace tray {
class PowerTrayView;
}

class ASH_EXPORT TrayPower : public SystemTrayItem,
                             public PowerStatus::Observer {
 public:
  enum NotificationState {
    NOTIFICATION_NONE,

    // Low battery charge.
    NOTIFICATION_LOW_POWER,

    // Critically low battery charge.
    NOTIFICATION_CRITICAL,
  };

  // Time-based notification thresholds when on battery power.
  static const int kCriticalMinutes;
  static const int kLowPowerMinutes;
  static const int kNoWarningMinutes;

  // Percentage-based notification thresholds when using a low-power charger.
  static const int kCriticalPercentage;
  static const int kLowPowerPercentage;
  static const int kNoWarningPercentage;

  static const char kUsbNotificationId[];

  TrayPower(SystemTray* system_tray,
            message_center::MessageCenter* message_center);
  ~TrayPower() override;

  void NotifyUsbNotificationClosedByUser();

 private:
  friend class TrayPowerTest;

  // This enum is used for histogram. The existing values should not be removed,
  // and the new values should be added just before CHARGER_TYPE_COUNT.
  enum ChargerType {
    UNKNOWN_CHARGER,
    MAINS_CHARGER,
    USB_CHARGER,
    UNCONFIRMED_SPRING_CHARGER,
    SAFE_SPRING_CHARGER,
    CHARGER_TYPE_COUNT,
  };

  // Overridden from SystemTrayItem.
  views::View* CreateTrayView(LoginStatus status) override;
  views::View* CreateDefaultView(LoginStatus status) override;
  void DestroyTrayView() override;
  void DestroyDefaultView() override;
  void UpdateAfterLoginStatusChange(LoginStatus status) override;
  void UpdateAfterShelfAlignmentChange(ShelfAlignment alignment) override;

  // Overridden from PowerStatus::Observer.
  void OnPowerStatusChanged() override;

  // Shows a notification that a low-power USB charger has been connected.
  // Returns true if a notification was shown or explicitly hidden.
  bool MaybeShowUsbChargerNotification();

  // Shows a notification when dual-role devices are connected.
  void MaybeShowDualRoleNotification();

  // Sets |notification_state_|. Returns true if a notification should be shown.
  bool UpdateNotificationState();
  bool UpdateNotificationStateForRemainingTime();
  bool UpdateNotificationStateForRemainingPercentage();

  message_center::MessageCenter* message_center_;  // Not owned.
  tray::PowerTrayView* power_tray_;
  std::unique_ptr<BatteryNotification> battery_notification_;
  std::unique_ptr<DualRoleNotification> dual_role_notification_;
  NotificationState notification_state_;

  // Was a USB charger connected the last time OnPowerStatusChanged() was
  // called?
  bool usb_charger_was_connected_;

  // Was line power connected the last time onPowerStatusChanged() was called?
  bool line_power_was_connected_;

  // Has the user already dismissed a low-power notification? Should be set
  // back to false when all power sources are disconnected.
  bool usb_notification_dismissed_;

  DISALLOW_COPY_AND_ASSIGN(TrayPower);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_CHROMEOS_POWER_TRAY_POWER_H_
