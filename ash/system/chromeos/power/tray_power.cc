// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/power/tray_power.h"

#include "ash/accessibility_delegate.h"
#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/system/chromeos/power/power_status_view.h"
#include "ash/system/date/date_view.h"
#include "ash/system/system_notifier.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_notification_view.h"
#include "ash/system/tray/tray_utils.h"
#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/time/time.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "third_party/icu/source/i18n/unicode/fieldpos.h"
#include "third_party/icu/source/i18n/unicode/fmtable.h"
#include "ui/accessibility/ax_view_state.h"
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
namespace tray {
namespace {

const int kMaxSpringChargerAccessibilityNotifyCount = 3;
const int kSpringChargerAccessibilityTimerFirstTimeNotifyInSeconds = 30;
const int kSpringChargerAccessibilityTimerRepeatInMinutes = 5;

}

// This view is used only for the tray.
class PowerTrayView : public views::ImageView {
 public:
  PowerTrayView()
      : spring_charger_spoken_notification_count_(0) {
    UpdateImage();
  }

  virtual ~PowerTrayView() {
  }

  // Overriden from views::View.
  virtual void GetAccessibleState(ui::AXViewState* state) OVERRIDE {
    state->name = accessible_name_;
    state->role = ui::AX_ROLE_BUTTON;
  }

  void UpdateStatus(bool battery_alert) {
    UpdateImage();
    SetVisible(PowerStatus::Get()->IsBatteryPresent());

    if (battery_alert) {
      accessible_name_ = PowerStatus::Get()->GetAccessibleNameString(true);
      NotifyAccessibilityEvent(ui::AX_EVENT_ALERT, true);
    }
  }

  void SetupNotifyBadCharger() {
    // Poll with a shorter duration timer to notify the charger issue
    // for the first time after the charger dialog is displayed.
    spring_charger_accessility_timer_.Start(
        FROM_HERE, base::TimeDelta::FromSeconds(
            kSpringChargerAccessibilityTimerFirstTimeNotifyInSeconds),
        this, &PowerTrayView::NotifyChargerIssue);
  }

 private:
  void UpdateImage() {
    SetImage(PowerStatus::Get()->GetBatteryImage(PowerStatus::ICON_LIGHT));
  }

  void NotifyChargerIssue() {
    if (!Shell::GetInstance()->accessibility_delegate()->
            IsSpokenFeedbackEnabled())
      return;

    if (!Shell::GetInstance()->system_tray_delegate()->
            IsSpringChargerReplacementDialogVisible()) {
      spring_charger_accessility_timer_.Stop();
      return;
    }

    accessible_name_ =  ui::ResourceBundle::GetSharedInstance().
        GetLocalizedString(IDS_CHARGER_REPLACEMENT_ACCESSIBILTY_NOTIFICATION);
    NotifyAccessibilityEvent(ui::AX_EVENT_ALERT, true);
    ++spring_charger_spoken_notification_count_;

    if (spring_charger_spoken_notification_count_ == 1) {
      // After notify the charger issue for the first time, repeat the
      // notification with a longer duration timer.
      spring_charger_accessility_timer_.Stop();
      spring_charger_accessility_timer_.Start(
          FROM_HERE, base::TimeDelta::FromMinutes(
              kSpringChargerAccessibilityTimerRepeatInMinutes),
          this, &PowerTrayView::NotifyChargerIssue);
    } else if (spring_charger_spoken_notification_count_ >=
        kMaxSpringChargerAccessibilityNotifyCount) {
      spring_charger_accessility_timer_.Stop();
    }
  }

  base::string16 accessible_name_;

  // Tracks how many times the original spring charger accessibility
  // notification has been spoken.
  int spring_charger_spoken_notification_count_;

  base::RepeatingTimer<PowerTrayView> spring_charger_accessility_timer_;

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

const int TrayPower::kCriticalMinutes = 5;
const int TrayPower::kLowPowerMinutes = 15;
const int TrayPower::kNoWarningMinutes = 30;
const int TrayPower::kCriticalPercentage = 5;
const int TrayPower::kLowPowerPercentage = 10;
const int TrayPower::kNoWarningPercentage = 15;

TrayPower::TrayPower(SystemTray* system_tray, MessageCenter* message_center)
    : SystemTrayItem(system_tray),
      message_center_(message_center),
      power_tray_(NULL),
      notification_view_(NULL),
      notification_state_(NOTIFICATION_NONE),
      usb_charger_was_connected_(false),
      line_power_was_connected_(false) {
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
  RecordChargerType();

  if (PowerStatus::Get()->IsOriginalSpringChargerConnected()) {
    if (ash::Shell::GetInstance()->system_tray_delegate()->
            ShowSpringChargerReplacementDialog()) {
      power_tray_->SetupNotifyBadCharger();
    }
  }

  bool battery_alert = UpdateNotificationState();
  if (power_tray_)
    power_tray_->UpdateStatus(battery_alert);
  if (notification_view_)
    notification_view_->UpdateStatus();

  // Factory testing may place the battery into unusual states.
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kAshHideNotificationsForFactory))
    return;

  MaybeShowUsbChargerNotification();

  if (battery_alert)
    ShowNotificationView();
  else if (notification_state_ == NOTIFICATION_NONE)
    HideNotificationView();

  usb_charger_was_connected_ = PowerStatus::Get()->IsUsbChargerConnected();
  line_power_was_connected_ = PowerStatus::Get()->IsLinePowerConnected();
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
        rb.GetLocalizedString(
            IDS_ASH_STATUS_TRAY_LOW_POWER_CHARGER_MESSAGE_SHORT),
        rb.GetImageNamed(IDR_AURA_NOTIFICATION_LOW_POWER_CHARGER),
        base::string16(),
        message_center::NotifierId(
            message_center::NotifierId::SYSTEM_COMPONENT,
            system_notifier::kNotifierPower),
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
      status.IsMainsChargerConnected() ||
      status.IsOriginalSpringChargerConnected()) {
    notification_state_ = NOTIFICATION_NONE;
    return false;
  }

  return status.IsUsbChargerConnected() ?
      UpdateNotificationStateForRemainingPercentage() :
      UpdateNotificationStateForRemainingTime();
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
        return true;
      }
      if (remaining_minutes <= kLowPowerMinutes) {
        notification_state_ = NOTIFICATION_LOW_POWER;
        return true;
      }
      return false;
    case NOTIFICATION_LOW_POWER:
      if (remaining_minutes <= kCriticalMinutes) {
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

void TrayPower::RecordChargerType() {
  if (!PowerStatus::Get()->IsLinePowerConnected() ||
      line_power_was_connected_)
    return;

  ChargerType current_charger = UNKNOWN_CHARGER;
  if (PowerStatus::Get()->IsMainsChargerConnected()) {
    current_charger = MAINS_CHARGER;
  } else if (PowerStatus::Get()->IsUsbChargerConnected()) {
    current_charger = USB_CHARGER;
  } else if (PowerStatus::Get()->IsOriginalSpringChargerConnected()) {
    current_charger =
        ash::Shell::GetInstance()->system_tray_delegate()->
            HasUserConfirmedSafeSpringCharger() ?
        SAFE_SPRING_CHARGER : UNCONFIRMED_SPRING_CHARGER;
  }

  if (current_charger != UNKNOWN_CHARGER) {
    UMA_HISTOGRAM_ENUMERATION("Power.ChargerType",
                              current_charger,
                              CHARGER_TYPE_COUNT);
  }
}

}  // namespace ash
