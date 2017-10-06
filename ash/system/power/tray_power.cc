// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/tray_power.h"

#include <utility>

#include "ash/accessibility/accessibility_delegate.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/resources/grit/ash_resources.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/date/date_view.h"
#include "ash/system/power/battery_notification.h"
#include "ash/system/power/dual_role_notification.h"
#include "ash/system/system_notifier.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_item_view.h"
#include "ash/system/tray/tray_utils.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/time/time.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_delegate.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"

using message_center::MessageCenter;
using message_center::Notification;

namespace ash {

// Informs the TrayPower instance when a USB notification is closed.
class UsbNotificationDelegate : public message_center::NotificationDelegate {
 public:
  explicit UsbNotificationDelegate(TrayPower* tray_power)
      : tray_power_(tray_power) {}

  // Overridden from message_center::NotificationDelegate.
  void Close(bool by_user) override {
    if (by_user)
      tray_power_->NotifyUsbNotificationClosedByUser();
  }

 private:
  ~UsbNotificationDelegate() override {}

  TrayPower* tray_power_;

  DISALLOW_COPY_AND_ASSIGN(UsbNotificationDelegate);
};

namespace {

std::string GetNotificationStateString(
    TrayPower::NotificationState notification_state) {
  switch (notification_state) {
    case TrayPower::NOTIFICATION_NONE:
      return "none";
    case TrayPower::NOTIFICATION_LOW_POWER:
      return "low power";
    case TrayPower::NOTIFICATION_CRITICAL:
      return "critical power";
  }
  NOTREACHED() << "Unknown state " << notification_state;
  return "Unknown state";
}

void LogBatteryForUsbCharger(TrayPower::NotificationState state,
                             int battery_percent) {
  LOG(WARNING) << "Showing " << GetNotificationStateString(state)
               << " notification. USB charger is connected. "
               << "Battery percentage: " << battery_percent << "%.";
}

void LogBatteryForNoCharger(TrayPower::NotificationState state,
                            int remaining_minutes) {
  LOG(WARNING) << "Showing " << GetNotificationStateString(state)
               << " notification. No charger connected."
               << " Remaining time: " << remaining_minutes << " minutes.";
}

}  // namespace

namespace tray {

// This view is used only for the tray.
class PowerTrayView : public TrayItemView {
 public:
  explicit PowerTrayView(SystemTrayItem* owner) : TrayItemView(owner) {
    CreateImageView();
    UpdateImage();
  }

  ~PowerTrayView() override {}

  // Overridden from views::View.
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->SetName(accessible_name_);
    node_data->role = ui::AX_ROLE_BUTTON;
  }

  void UpdateStatus(bool battery_alert) {
    UpdateImage();
    SetVisible(PowerStatus::Get()->IsBatteryPresent());

    if (battery_alert) {
      accessible_name_ = PowerStatus::Get()->GetAccessibleNameString(true);
      NotifyAccessibilityEvent(ui::AX_EVENT_ALERT, true);
    }
  }

 private:
  void UpdateImage() {
    const PowerStatus::BatteryImageInfo& info =
        PowerStatus::Get()->GetBatteryImageInfo();
    // Only change the image when the info changes. http://crbug.com/589348
    if (info_ && info_->ApproximatelyEqual(info))
      return;
    image_view()->SetImage(PowerStatus::GetBatteryImage(
        info, kTrayIconSize, SkColorSetA(kTrayIconColor, 0x4C),
        kTrayIconColor));
  }

  base::string16 accessible_name_;
  base::Optional<PowerStatus::BatteryImageInfo> info_;

  DISALLOW_COPY_AND_ASSIGN(PowerTrayView);
};

}  // namespace tray

const int TrayPower::kCriticalMinutes = 5;
const int TrayPower::kLowPowerMinutes = 15;
const int TrayPower::kNoWarningMinutes = 30;
const int TrayPower::kCriticalPercentage = 5;
const int TrayPower::kLowPowerPercentage = 10;
const int TrayPower::kNoWarningPercentage = 15;

const char TrayPower::kUsbNotificationId[] = "usb-charger";

TrayPower::TrayPower(SystemTray* system_tray, MessageCenter* message_center)
    : SystemTrayItem(system_tray, UMA_POWER),
      message_center_(message_center),
      power_tray_(nullptr),
      notification_state_(NOTIFICATION_NONE),
      usb_charger_was_connected_(false),
      line_power_was_connected_(false),
      usb_notification_dismissed_(false) {
  PowerStatus::Get()->AddObserver(this);
}

TrayPower::~TrayPower() {
  PowerStatus::Get()->RemoveObserver(this);
  message_center_->RemoveNotification(kUsbNotificationId, false);
}

views::View* TrayPower::CreateTrayView(LoginStatus status) {
  // There may not be enough information when this is created about whether
  // there is a battery or not. So always create this, and adjust visibility as
  // necessary.
  CHECK(power_tray_ == nullptr);
  power_tray_ = new tray::PowerTrayView(this);
  power_tray_->UpdateStatus(false);
  return power_tray_;
}

views::View* TrayPower::CreateDefaultView(LoginStatus status) {
  // Make sure icon status is up to date. (Also triggers stub activation).
  PowerStatus::Get()->RequestStatusUpdate();
  return nullptr;
}

void TrayPower::OnTrayViewDestroyed() {
  power_tray_ = nullptr;
}

void TrayPower::OnPowerStatusChanged() {
  bool battery_alert = UpdateNotificationState();
  if (power_tray_)
    power_tray_->UpdateStatus(battery_alert);

  // Factory testing may place the battery into unusual states.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kAshHideNotificationsForFactory))
    return;

  MaybeShowUsbChargerNotification();
  MaybeShowDualRoleNotification();

  if (battery_alert) {
    // Remove any existing notification so it's dismissed before adding a new
    // one. Otherwise we might update a "low battery" notification to "critical"
    // without it being shown again.
    battery_notification_.reset();
    battery_notification_.reset(
        new BatteryNotification(message_center_, notification_state_));
  } else if (notification_state_ == NOTIFICATION_NONE) {
    battery_notification_.reset();
  } else if (battery_notification_.get()) {
    battery_notification_->Update(notification_state_);
  }

  usb_charger_was_connected_ = PowerStatus::Get()->IsUsbChargerConnected();
  line_power_was_connected_ = PowerStatus::Get()->IsLinePowerConnected();
}

bool TrayPower::MaybeShowUsbChargerNotification() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  const PowerStatus& status = *PowerStatus::Get();

  bool usb_charger_is_connected = status.IsUsbChargerConnected();

  // Check for a USB charger being connected.
  if (usb_charger_is_connected && !usb_charger_was_connected_ &&
      !usb_notification_dismissed_) {
    std::unique_ptr<Notification> notification =
        system_notifier::CreateSystemNotification(
            message_center::NOTIFICATION_TYPE_SIMPLE, kUsbNotificationId,
            rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_LOW_POWER_CHARGER_TITLE),
            ui::SubstituteChromeOSDeviceType(
                IDS_ASH_STATUS_TRAY_LOW_POWER_CHARGER_MESSAGE_SHORT),
            rb.GetImageNamed(IDR_AURA_NOTIFICATION_LOW_POWER_CHARGER),
            base::string16(), GURL(),
            message_center::NotifierId(
                message_center::NotifierId::SYSTEM_COMPONENT,
                system_notifier::kNotifierPower),
            message_center::RichNotificationData(),
            new UsbNotificationDelegate(this), kNotificationLowPowerBatteryIcon,
            message_center::SystemNotificationWarningLevel::WARNING);
    // TODO(tetsui): Workaround of https://crbug.com/757724. Remove after the
    // bug is fixed.
    notification->set_vector_small_image(gfx::kNoneIcon);
    message_center_->AddNotification(std::move(notification));
    return true;
  } else if (!usb_charger_is_connected && usb_charger_was_connected_) {
    // USB charger was unplugged or was identified as a different type while
    // the USB charger notification was showing.
    message_center_->RemoveNotification(kUsbNotificationId, false);
    if (!status.IsLinePowerConnected())
      usb_notification_dismissed_ = false;
    return true;
  }
  return false;
}

void TrayPower::MaybeShowDualRoleNotification() {
  const PowerStatus& status = *PowerStatus::Get();
  if (!status.HasDualRoleDevices()) {
    dual_role_notification_.reset();
    return;
  }

  if (!dual_role_notification_)
    dual_role_notification_.reset(new DualRoleNotification(message_center_));
  dual_role_notification_->Update();
}

bool TrayPower::UpdateNotificationState() {
  const PowerStatus& status = *PowerStatus::Get();
  if (!status.IsBatteryPresent() || status.IsBatteryTimeBeingCalculated() ||
      status.IsMainsChargerConnected()) {
    notification_state_ = NOTIFICATION_NONE;
    return false;
  }

  return status.IsUsbChargerConnected()
             ? UpdateNotificationStateForRemainingPercentage()
             : UpdateNotificationStateForRemainingTime();
}

bool TrayPower::UpdateNotificationStateForRemainingTime() {
  // The notification includes a rounded minutes value, so round the estimate
  // received from the power manager to match.
  const int remaining_minutes = static_cast<int>(
      PowerStatus::Get()->GetBatteryTimeToEmpty().InSecondsF() / 60.0 + 0.5);

  if (remaining_minutes >= kNoWarningMinutes ||
      PowerStatus::Get()->IsBatteryFull()) {
    notification_state_ = NOTIFICATION_NONE;
    return false;
  }

  switch (notification_state_) {
    case NOTIFICATION_NONE:
      if (remaining_minutes <= kCriticalMinutes) {
        notification_state_ = NOTIFICATION_CRITICAL;
        LogBatteryForNoCharger(notification_state_, remaining_minutes);
        return true;
      }
      if (remaining_minutes <= kLowPowerMinutes) {
        notification_state_ = NOTIFICATION_LOW_POWER;
        LogBatteryForNoCharger(notification_state_, remaining_minutes);
        return true;
      }
      return false;
    case NOTIFICATION_LOW_POWER:
      if (remaining_minutes <= kCriticalMinutes) {
        notification_state_ = NOTIFICATION_CRITICAL;
        LogBatteryForNoCharger(notification_state_, remaining_minutes);
        return true;
      }
      return false;
    case NOTIFICATION_CRITICAL:
      return false;
  }
  NOTREACHED();
  return false;
}

bool TrayPower::UpdateNotificationStateForRemainingPercentage() {
  // The notification includes a rounded percentage, so round the value received
  // from the power manager to match.
  const int remaining_percentage =
      PowerStatus::Get()->GetRoundedBatteryPercent();

  if (remaining_percentage >= kNoWarningPercentage ||
      PowerStatus::Get()->IsBatteryFull()) {
    notification_state_ = NOTIFICATION_NONE;
    return false;
  }

  switch (notification_state_) {
    case NOTIFICATION_NONE:
      if (remaining_percentage <= kCriticalPercentage) {
        notification_state_ = NOTIFICATION_CRITICAL;
        LogBatteryForUsbCharger(notification_state_, remaining_percentage);
        return true;
      }
      if (remaining_percentage <= kLowPowerPercentage) {
        notification_state_ = NOTIFICATION_LOW_POWER;
        LogBatteryForUsbCharger(notification_state_, remaining_percentage);
        return true;
      }
      return false;
    case NOTIFICATION_LOW_POWER:
      if (remaining_percentage <= kCriticalPercentage) {
        notification_state_ = NOTIFICATION_CRITICAL;
        LogBatteryForUsbCharger(notification_state_, remaining_percentage);
        return true;
      }
      return false;
    case NOTIFICATION_CRITICAL:
      return false;
  }
  NOTREACHED();
  return false;
}

void TrayPower::NotifyUsbNotificationClosedByUser() {
  usb_notification_dismissed_ = true;
}

}  // namespace ash
