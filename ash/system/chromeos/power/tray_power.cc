// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/power/tray_power.h"

#include "ash/ash_switches.h"
#include "ash/system/chromeos/power/power_status_view.h"
#include "ash/system/date/date_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_notification_view.h"
#include "ash/system/tray/tray_utils.h"
#include "base/command_line.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "third_party/icu/source/i18n/unicode/fieldpos.h"
#include "third_party/icu/source/i18n/unicode/fmtable.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/resource/resource_bundle.h"
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

using message_center::MessageCenter;
using message_center::Notification;

namespace ash {
namespace internal {

namespace {
// Top/bottom padding of the text items.
const int kPaddingVertical = 10;
// Specify min width of status label for layout.
const int kLabelMinWidth = 120;
// Notification times.
const int kCriticalSeconds = 5 * 60;
const int kLowPowerSeconds = 15 * 60;
const int kNoWarningSeconds = 30 * 60;
// Notification in battery percentage.
const double kCriticalPercentage = 5.0;
const double kLowPowerPercentage = 10.0;
const double kNoWarningPercentage = 15.0;

}  // namespace

namespace tray {

// This view is used only for the tray.
class PowerTrayView : public views::ImageView {
 public:
  PowerTrayView() {
    UpdateImage();
  }

  virtual ~PowerTrayView() {
  }

  // Overriden from views::View.
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE {
    state->name = accessible_name_;
    state->role = ui::AccessibilityTypes::ROLE_PUSHBUTTON;
  }

  void UpdateStatus(bool battery_alert) {
    UpdateImage();
    SetVisible(PowerStatus::Get()->IsBatteryPresent());

    if (battery_alert) {
      accessible_name_ = PowerStatus::Get()->GetAccessibleNameString();
      NotifyAccessibilityEvent(ui::AccessibilityTypes::EVENT_ALERT, true);
    }
  }

 private:
  void UpdateImage() {
    SetImage(PowerStatus::Get()->GetBatteryImage(PowerStatus::ICON_LIGHT));
  }

  base::string16 accessible_name_;

  DISALLOW_COPY_AND_ASSIGN(PowerTrayView);
};

class PowerNotificationView : public TrayNotificationView {
 public:
  explicit PowerNotificationView(TrayPower* owner)
      : TrayNotificationView(owner, 0) {
    power_status_view_ =
        new PowerStatusView(PowerStatusView::VIEW_NOTIFICATION, true);
    InitView(power_status_view_);
  }

  void UpdateStatus() {
    SetIconImage(PowerStatus::Get()->GetBatteryImage(PowerStatus::ICON_DARK));
  }

 private:
  PowerStatusView* power_status_view_;

  DISALLOW_COPY_AND_ASSIGN(PowerNotificationView);
};

}  // namespace tray

using tray::PowerNotificationView;

TrayPower::TrayPower(SystemTray* system_tray, MessageCenter* message_center)
    : SystemTrayItem(system_tray),
      message_center_(message_center),
      power_tray_(NULL),
      notification_view_(NULL),
      notification_state_(NOTIFICATION_NONE),
      usb_charger_was_connected_(false) {
  PowerStatus::Get()->AddObserver(this);
}

TrayPower::~TrayPower() {
  PowerStatus::Get()->RemoveObserver(this);
}

views::View* TrayPower::CreateTrayView(user::LoginStatus status) {
  // There may not be enough information when this is created about whether
  // there is a battery or not. So always create this, and adjust visibility as
  // necessary.
  CHECK(power_tray_ == NULL);
  power_tray_ = new tray::PowerTrayView();
  power_tray_->UpdateStatus(false);
  return power_tray_;
}

views::View* TrayPower::CreateDefaultView(user::LoginStatus status) {
  // Make sure icon status is up-to-date. (Also triggers stub activation).
  PowerStatus::Get()->RequestStatusUpdate();
  return NULL;
}

views::View* TrayPower::CreateNotificationView(user::LoginStatus status) {
  CHECK(notification_view_ == NULL);
  if (!PowerStatus::Get()->IsBatteryPresent())
    return NULL;

  notification_view_ = new PowerNotificationView(this);
  notification_view_->UpdateStatus();

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

void TrayPower::OnPowerStatusChanged() {
  bool battery_alert = UpdateNotificationState();
  if (power_tray_)
    power_tray_->UpdateStatus(battery_alert);
  if (notification_view_)
    notification_view_->UpdateStatus();

  // Factory testing may place the battery into unusual states.
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kAshHideNotificationsForFactory))
    return;

  if (ash::switches::UseUsbChargerNotification())
    MaybeShowUsbChargerNotification();

  if (battery_alert)
    ShowNotificationView();
  else if (notification_state_ == NOTIFICATION_NONE)
    HideNotificationView();

  usb_charger_was_connected_ = PowerStatus::Get()->IsUsbChargerConnected();
}

bool TrayPower::MaybeShowUsbChargerNotification() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  const char kNotificationId[] = "usb-charger";
  bool usb_charger_is_connected = PowerStatus::Get()->IsUsbChargerConnected();

  // Check for a USB charger being connected.
  if (usb_charger_is_connected && !usb_charger_was_connected_) {
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
  if (!usb_charger_is_connected && usb_charger_was_connected_) {
    message_center_->RemoveNotification(kNotificationId, false);
    return true;
  }
  return false;
}

bool TrayPower::UpdateNotificationState() {
  const PowerStatus& status = *PowerStatus::Get();
  if (!status.IsBatteryPresent() ||
      status.IsBatteryTimeBeingCalculated() ||
      status.IsMainsChargerConnected()) {
    notification_state_ = NOTIFICATION_NONE;
    return false;
  }

  return status.IsUsbChargerConnected() ?
      UpdateNotificationStateForRemainingPercentage() :
      UpdateNotificationStateForRemainingTime();
}

bool TrayPower::UpdateNotificationStateForRemainingTime() {
  const int remaining_seconds =
      PowerStatus::Get()->GetBatteryTimeToEmpty().InSeconds();

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

bool TrayPower::UpdateNotificationStateForRemainingPercentage() {
  const double remaining_percentage = PowerStatus::Get()->GetBatteryPercent();

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
