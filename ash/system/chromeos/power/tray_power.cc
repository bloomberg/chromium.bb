// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/power/tray_power.h"

#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/system/chromeos/power/power_status_view.h"
#include "ash/system/date/date_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_notification_view.h"
#include "ash/system/tray/tray_utils.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/dbus/power_supply_status.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "third_party/icu/public/i18n/unicode/fieldpos.h"
#include "third_party/icu/public/i18n/unicode/fmtable.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/size.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

using chromeos::PowerManagerHandler;
using chromeos::PowerSupplyStatus;
using message_center::MessageCenter;
using message_center::Notification;

namespace ash {
namespace internal {

namespace {
// Width and height of battery images.
const int kBatteryImageHeight = 25;
const int kBatteryImageWidth = 25;
// Number of different power states.
const int kNumPowerImages = 15;
// Top/bottom padding of the text items.
const int kPaddingVertical = 10;
// Specify min width of status label for layout.
const int kLabelMinWidth = 120;
// Notification times.
const int kCriticalSeconds = 5 * 60;
const int kLowPowerSeconds = 15 * 60;
const int kNoWarningSeconds = 30 * 60;
// Minimum battery percentage rendered in UI.
const int kMinBatteryPercent = 1;
// Notification in battery percentage.
const double kCriticalPercentage = 5.0;
const double kLowPowerPercentage = 10.0;
const double kNoWarningPercentage = 15.0;

base::string16 GetBatteryTimeAccessibilityString(int hour, int min) {
  DCHECK(hour || min);
  if (hour && !min) {
    return Shell::GetInstance()->delegate()->GetTimeDurationLongString(
        base::TimeDelta::FromHours(hour));
  }
  if (min && !hour) {
    return Shell::GetInstance()->delegate()->GetTimeDurationLongString(
        base::TimeDelta::FromMinutes(min));
  }
  return l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_BATTERY_TIME_ACCESSIBLE,
      Shell::GetInstance()->delegate()->GetTimeDurationLongString(
          base::TimeDelta::FromHours(hour)),
      Shell::GetInstance()->delegate()->GetTimeDurationLongString(
          base::TimeDelta::FromMinutes(min)));
}

}  // namespace

namespace tray {

// This view is used only for the tray.
class PowerTrayView : public views::ImageView {
 public:
  PowerTrayView()
      : battery_icon_index_(-1),
        battery_icon_offset_(0),
        battery_charging_unreliable_(false) {
    UpdateImage();
  }

  virtual ~PowerTrayView() {
  }

  // Overriden from views::View.
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE {
    state->name = accessible_name_;
    state->role = ui::AccessibilityTypes::ROLE_PUSHBUTTON;
  }

  void UpdatePowerStatus(const PowerSupplyStatus& status,
                         bool battery_alert) {
    supply_status_ = status;
    // Sanitize.
    if (supply_status_.battery_is_full)
      supply_status_.battery_percentage = 100.0;

    UpdateImage();
    SetVisible(status.battery_is_present);

    if (battery_alert) {
      accessible_name_ = TrayPower::GetAccessibleNameString(status);
      NotifyAccessibilityEvent(ui::AccessibilityTypes::EVENT_ALERT, true);
    }
  }

 private:
  void UpdateImage() {
    int index = TrayPower::GetBatteryImageIndex(supply_status_);
    int offset = TrayPower::GetBatteryImageOffset(supply_status_);
    bool charging_unreliable =
        TrayPower::IsBatteryChargingUnreliable(supply_status_);
    if (battery_icon_index_ != index ||
        battery_icon_offset_ != offset ||
        battery_charging_unreliable_ != charging_unreliable) {
      battery_icon_index_ = index;
      battery_icon_offset_ = offset;
      battery_charging_unreliable_ = charging_unreliable;
      if (battery_icon_index_ != -1)
        SetImage(TrayPower::GetBatteryImage(battery_icon_index_,
                                            battery_icon_offset_,
                                            battery_charging_unreliable_,
                                            ICON_LIGHT));
    }
  }

  PowerSupplyStatus supply_status_;
  base::string16 accessible_name_;

  // Index of the current icon in the icon array image, or -1 if unknown.
  int battery_icon_index_;
  int battery_icon_offset_;
  bool battery_charging_unreliable_;

  DISALLOW_COPY_AND_ASSIGN(PowerTrayView);
};

class PowerNotificationView : public TrayNotificationView {
 public:
  explicit PowerNotificationView(TrayPower* owner)
      : TrayNotificationView(owner, 0),
        battery_icon_index_(-1),
        battery_icon_offset_(0),
        battery_charging_unreliable_(false) {
    power_status_view_ =
        new PowerStatusView(PowerStatusView::VIEW_NOTIFICATION, true);
    InitView(power_status_view_);
  }

  void UpdatePowerStatus(const PowerSupplyStatus& status) {
    int index = TrayPower::GetBatteryImageIndex(status);
    int offset = TrayPower::GetBatteryImageOffset(status);
    bool charging_unreliable = TrayPower::IsBatteryChargingUnreliable(status);
    if (battery_icon_index_ != index ||
        battery_icon_offset_ != offset ||
        battery_charging_unreliable_ != charging_unreliable) {
      battery_icon_index_ = index;
      battery_icon_offset_ = offset;
      battery_charging_unreliable_ = charging_unreliable;
      if (battery_icon_index_ != -1) {
        SetIconImage(TrayPower::GetBatteryImage(
                         battery_icon_index_,
                         battery_icon_offset_,
                         battery_charging_unreliable_,
                         ICON_DARK));
      }
    }
    power_status_view_->UpdatePowerStatus(status);
  }

 private:
  PowerStatusView* power_status_view_;

  // Index of the current icon in the icon array image, or -1 if unknown.
  int battery_icon_index_;
  int battery_icon_offset_;
  bool battery_charging_unreliable_;

  DISALLOW_COPY_AND_ASSIGN(PowerNotificationView);
};

}  // namespace tray

using tray::PowerNotificationView;

TrayPower::TrayPower(SystemTray* system_tray, MessageCenter* message_center)
    : SystemTrayItem(system_tray),
      message_center_(message_center),
      power_tray_(NULL),
      notification_view_(NULL),
      notification_state_(NOTIFICATION_NONE) {
  // Tests may not have a PowerManagerHandler.
  if (PowerManagerHandler::IsInitialized())
    PowerManagerHandler::Get()->AddObserver(this);
}

TrayPower::~TrayPower() {
  if (PowerManagerHandler::IsInitialized())
    PowerManagerHandler::Get()->RemoveObserver(this);
}

// static
bool TrayPower::IsBatteryChargingUnreliable(
    const chromeos::PowerSupplyStatus& supply_status) {
  // Sometimes devices can get into a state where the battery is almost fully
  // charged and the power subsystem reports "neither charging nor discharging"
  // despite the battery not at 100%. For now, only report unreliable charging
  // on USB.
  // TODO(derat): Update this when the power manager code is refactored for M29.
  return supply_status.battery_state == PowerSupplyStatus::CONNECTED_TO_USB;
}

// static
int TrayPower::GetBatteryImageIndex(
    const chromeos::PowerSupplyStatus& supply_status) {
  int image_index = 0;
  if (supply_status.battery_percentage >= 100) {
    image_index = kNumPowerImages - 1;
  } else if (!supply_status.battery_is_present) {
    image_index = kNumPowerImages;
  } else {
    image_index = static_cast<int>(supply_status.battery_percentage /
                                   100.0 * (kNumPowerImages - 1));
    image_index = std::max(std::min(image_index, kNumPowerImages - 2), 0);
  }
  return image_index;
}

// static
int TrayPower::GetBatteryImageOffset(
    const chromeos::PowerSupplyStatus& supply_status) {
  if (IsBatteryChargingUnreliable(supply_status) ||
      !supply_status.line_power_on)
    return 0;
  return 1;
}

// static
gfx::ImageSkia TrayPower::GetBatteryImage(int image_index,
                                          int image_offset,
                                          bool charging_unreliable,
                                          IconSet icon_set) {
  gfx::Image all;
  if (charging_unreliable) {
    all = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        icon_set == ICON_DARK ?
        IDR_AURA_UBER_TRAY_POWER_SMALL_CHARGING_UNRELIABLE_DARK :
        IDR_AURA_UBER_TRAY_POWER_SMALL_CHARGING_UNRELIABLE);
  } else {
    all = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        icon_set == ICON_DARK ?
        IDR_AURA_UBER_TRAY_POWER_SMALL_DARK : IDR_AURA_UBER_TRAY_POWER_SMALL);
  }
  gfx::Rect region(
      image_offset * kBatteryImageWidth,
      image_index * kBatteryImageHeight,
      kBatteryImageWidth, kBatteryImageHeight);
  return gfx::ImageSkiaOperations::ExtractSubset(*all.ToImageSkia(), region);
}

// static
base::string16 TrayPower::GetAccessibleNameString(
    const chromeos::PowerSupplyStatus& supply_status) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  if (supply_status.line_power_on && supply_status.battery_is_full) {
    return rb.GetLocalizedString(
        IDS_ASH_STATUS_TRAY_BATTERY_FULL_CHARGE_ACCESSIBLE);
  }
  bool charging_unreliable =
      IsBatteryChargingUnreliable(supply_status);
  if (supply_status.battery_percentage < 0.0f) {
    if (charging_unreliable) {
      return rb.GetLocalizedString(
          IDS_ASH_STATUS_TRAY_BATTERY_CHARGING_UNRELIABLE_ACCESSIBLE);
    }
    return rb.GetLocalizedString(
        IDS_ASH_STATUS_TRAY_BATTERY_CALCULATING_ACCESSIBLE);
  }
  base::string16 battery_percentage_accessbile = l10n_util::GetStringFUTF16(
      supply_status.line_power_on ?
      IDS_ASH_STATUS_TRAY_BATTERY_PERCENT_CHARGING_ACCESSIBLE:
      IDS_ASH_STATUS_TRAY_BATTERY_PERCENT_ACCESSIBLE ,
      base::IntToString16(GetRoundedBatteryPercentage(
          supply_status.battery_percentage)));
  base::string16 battery_time_accessible = base::string16();
  if (charging_unreliable) {
    battery_time_accessible = rb.GetLocalizedString(
        IDS_ASH_STATUS_TRAY_BATTERY_CHARGING_UNRELIABLE_ACCESSIBLE);
  } else {
    if (supply_status.is_calculating_battery_time) {
      battery_time_accessible = rb.GetLocalizedString(
          IDS_ASH_STATUS_TRAY_BATTERY_CALCULATING_ACCESSIBLE);
    } else {
      base::TimeDelta time = base::TimeDelta::FromSeconds(
          supply_status.line_power_on ?
          supply_status.battery_seconds_to_full :
          supply_status.battery_seconds_to_empty);
      int hour = time.InHours();
      int min = (time - base::TimeDelta::FromHours(hour)).InMinutes();
      if (hour || min) {
        base::string16 minute = min < 10 ?
            ASCIIToUTF16("0") + base::IntToString16(min) :
            base::IntToString16(min);
        battery_time_accessible =
            l10n_util::GetStringFUTF16(
                supply_status.line_power_on ?
                IDS_ASH_STATUS_TRAY_BATTERY_TIME_UNTIL_FULL_ACCESSIBLE :
                IDS_ASH_STATUS_TRAY_BATTERY_TIME_LEFT_ACCESSIBLE,
                GetBatteryTimeAccessibilityString(hour, min));
      }
    }
  }
  return battery_time_accessible.empty() ?
      battery_percentage_accessbile :
      battery_percentage_accessbile + ASCIIToUTF16(". ")
      + battery_time_accessible;
}

// static
int TrayPower::GetRoundedBatteryPercentage(double battery_percentage) {
  DCHECK(battery_percentage >= 0.0);
  return std::max(kMinBatteryPercent,
      static_cast<int>(battery_percentage + 0.5));
}

views::View* TrayPower::CreateTrayView(user::LoginStatus status) {
  // There may not be enough information when this is created about whether
  // there is a battery or not. So always create this, and adjust visibility as
  // necessary.
  PowerSupplyStatus power_status =
      PowerManagerHandler::Get()->GetPowerSupplyStatus();
  CHECK(power_tray_ == NULL);
  power_tray_ = new tray::PowerTrayView();
  power_tray_->UpdatePowerStatus(power_status, false);
  return power_tray_;
}

views::View* TrayPower::CreateDefaultView(user::LoginStatus status) {
  // Make sure icon status is up-to-date. (Also triggers stub activation).
  RequestStatusUpdate();
  return NULL;
}

views::View* TrayPower::CreateNotificationView(user::LoginStatus status) {
  CHECK(notification_view_ == NULL);
  PowerSupplyStatus power_status =
      PowerManagerHandler::Get()->GetPowerSupplyStatus();
  if (!power_status.battery_is_present)
    return NULL;

  notification_view_ = new PowerNotificationView(this);
  notification_view_->UpdatePowerStatus(power_status);

  return notification_view_;
}

void TrayPower::DestroyTrayView() {
  power_tray_ = NULL;
}

void TrayPower::DestroyDefaultView() {
}

void TrayPower::DestroyNotificationView() {
  notification_view_ = NULL;
}

void TrayPower::UpdateAfterLoginStatusChange(user::LoginStatus status) {
}

void TrayPower::UpdateAfterShelfAlignmentChange(ShelfAlignment alignment) {
  SetTrayImageItemBorder(power_tray_, alignment);
}

void TrayPower::OnPowerStatusChanged(
    const chromeos::PowerSupplyStatus& status) {
  bool battery_alert = UpdateNotificationState(status);
  if (power_tray_)
    power_tray_->UpdatePowerStatus(status, battery_alert);
  if (notification_view_)
    notification_view_->UpdatePowerStatus(status);

  // Factory testing may place the battery into unusual states.
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kAshHideNotificationsForFactory))
    return;

  if (ash::switches::UseUsbChargerNotification())
    MaybeShowUsbChargerNotification(last_power_supply_status_, status);

  if (battery_alert)
    ShowNotificationView();
  else if (notification_state_ == NOTIFICATION_NONE)
    HideNotificationView();

  last_power_supply_status_ = status;
}

void TrayPower::RequestStatusUpdate() const {
  PowerManagerHandler::Get()->RequestStatusUpdate();
}

bool TrayPower::MaybeShowUsbChargerNotification(
    const PowerSupplyStatus& old_status,
    const PowerSupplyStatus& new_status) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  const char kNotificationId[] = "usb-charger";
  // Check for a USB charger being connected.
  if (new_status.battery_state == PowerSupplyStatus::CONNECTED_TO_USB &&
      old_status.battery_state != PowerSupplyStatus::CONNECTED_TO_USB) {
    scoped_ptr<Notification> notification(new Notification(
        message_center::NOTIFICATION_TYPE_SIMPLE,
        kNotificationId,
        rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_LOW_POWER_CHARGER_TITLE),
        rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_LOW_POWER_CHARGER_MESSAGE),
        rb.GetImageNamed(IDR_AURA_NOTIFICATION_LOW_POWER_CHARGER),
        base::string16(),
        std::string(),
        message_center::RichNotificationData(),
        NULL));
    message_center_->AddNotification(notification.Pass());
    return true;
  }

  // Check for unplug of a USB charger while the USB charger notification is
  // showing.
  if (new_status.battery_state != PowerSupplyStatus::CONNECTED_TO_USB &&
      old_status.battery_state == PowerSupplyStatus::CONNECTED_TO_USB) {
    message_center_->RemoveNotification(kNotificationId, false);
    return true;
  }
  return false;
}

bool TrayPower::UpdateNotificationState(
    const chromeos::PowerSupplyStatus& status) {
  if (!status.battery_is_present ||
      status.is_calculating_battery_time ||
      status.battery_state == PowerSupplyStatus::CHARGING ||
      status.battery_state ==
          PowerSupplyStatus::NEITHER_CHARGING_NOR_DISCHARGING) {
    notification_state_ = NOTIFICATION_NONE;
    return false;
  }

  if (TrayPower::IsBatteryChargingUnreliable(status)) {
    return UpdateNotificationStateForRemainingPercentage(
        status.battery_percentage);
  } else {
    return UpdateNotificationStateForRemainingTime(
        status.battery_seconds_to_empty);
  }
}

bool TrayPower::UpdateNotificationStateForRemainingTime(int remaining_seconds) {
  if (remaining_seconds >= kNoWarningSeconds) {
    notification_state_ = NOTIFICATION_NONE;
    return false;
  }

  switch (notification_state_) {
    case NOTIFICATION_NONE:
      if (remaining_seconds <= kCriticalSeconds) {
        notification_state_ = NOTIFICATION_CRITICAL;
        return true;
      }
      if (remaining_seconds <= kLowPowerSeconds) {
        notification_state_ = NOTIFICATION_LOW_POWER;
        return true;
      }
      return false;
    case NOTIFICATION_LOW_POWER:
      if (remaining_seconds <= kCriticalSeconds) {
        notification_state_ = NOTIFICATION_CRITICAL;
        return true;
      }
      return false;
    case NOTIFICATION_CRITICAL:
      return false;
  }
  NOTREACHED();
  return false;
}

bool TrayPower::UpdateNotificationStateForRemainingPercentage(
    double remaining_percentage) {
  if (remaining_percentage > kNoWarningPercentage) {
    notification_state_ = NOTIFICATION_NONE;
    return false;
  }

  switch (notification_state_) {
    case NOTIFICATION_NONE:
      if (remaining_percentage <= kCriticalPercentage) {
        notification_state_ = NOTIFICATION_CRITICAL;
        return true;
      }
      if (remaining_percentage <= kLowPowerPercentage) {
        notification_state_ = NOTIFICATION_LOW_POWER;
        return true;
      }
      return false;
    case NOTIFICATION_LOW_POWER:
      if (remaining_percentage <= kCriticalPercentage) {
        notification_state_ = NOTIFICATION_CRITICAL;
        return true;
      }
      return false;
    case NOTIFICATION_CRITICAL:
      return false;
  }
  NOTREACHED();
  return false;
}

}  // namespace internal
}  // namespace ash
