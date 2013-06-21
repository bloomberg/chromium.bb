// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_POWER_TRAY_POWER_H_
#define ASH_SYSTEM_CHROMEOS_POWER_TRAY_POWER_H_

#include "ash/system/tray/system_tray_item.h"
#include "chromeos/power/power_manager_handler.h"

class SkBitmap;

namespace gfx {
class Image;
class ImageSkia;
}

namespace message_center {
class MessageCenter;
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

class ASH_EXPORT TrayPower : public SystemTrayItem,
                             public chromeos::PowerManagerHandler::Observer {
 public:
  // Visible for testing.
  enum NotificationState {
    NOTIFICATION_NONE,

    // Low battery charge.
    NOTIFICATION_LOW_POWER,

    // Critically low battery charge.
    NOTIFICATION_CRITICAL,
  };

  TrayPower(SystemTray* system_tray,
            message_center::MessageCenter* message_center);
  virtual ~TrayPower();

  // Gets whether battery charging is unreliable for |supply_status|.
  // When a non-standard power supply is connected, the battery may
  // change from being charged to discharged frequently depending on the
  // charger power and power consumption, i.e usage. In this case we
  // do not want to show either a charging or discharging state.
  static bool IsBatteryChargingUnreliable(
      const chromeos::PowerSupplyStatus& supply_status);

  // Gets the icon index in the battery icon array image based on
  // |supply_status|.  If |supply_status| is uncertain about the power state,
  // returns -1.
  static int GetBatteryImageIndex(
      const chromeos::PowerSupplyStatus& supply_status);

  // Gets the horizontal offset in the battery icon array image based on
  // |supply_status|.
  static int GetBatteryImageOffset(
      const chromeos::PowerSupplyStatus& supply_status);

  // Looks up the actual icon in the icon array image for |image_index|.
  static gfx::ImageSkia GetBatteryImage(int image_index,
                                        int image_offset,
                                        bool charging_unreliable,
                                        IconSet icon_set);

  // Gets the battery accessible string for |supply_status|.
  static base::string16 GetAccessibleNameString(
      const chromeos::PowerSupplyStatus& supply_status);

  // Gets rounded battery percentage for |battery_percentage|.
  static int GetRoundedBatteryPercentage(double battery_percentage);

 private:
  friend class TrayPowerTest;

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

  // Overridden from chromeos::PowerManagerHandler::Observer.
  virtual void OnPowerStatusChanged(
      const chromeos::PowerSupplyStatus& status) OVERRIDE;

  // Requests a power status update.
  void RequestStatusUpdate() const;

  // Show a notification that a low-power USB charger has been connected.
  // Returns true if a notification was shown or explicitly hidden.
  bool MaybeShowUsbChargerNotification(
      const chromeos::PowerSupplyStatus& old_status,
      const chromeos::PowerSupplyStatus& new_status);

  // Sets |notification_state_|. Returns true if a notification should be shown.
  bool UpdateNotificationState(const chromeos::PowerSupplyStatus& status);
  bool UpdateNotificationStateForRemainingTime(int remaining_seconds);
  bool UpdateNotificationStateForRemainingPercentage(
      double remaining_percentage);

  message_center::MessageCenter* message_center_;  // Not owned.
  tray::PowerTrayView* power_tray_;
  tray::PowerNotificationView* notification_view_;
  NotificationState notification_state_;

  // Power supply status at the last update.
  chromeos::PowerSupplyStatus last_power_supply_status_;

  DISALLOW_COPY_AND_ASSIGN(TrayPower);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_POWER_TRAY_POWER_H_
