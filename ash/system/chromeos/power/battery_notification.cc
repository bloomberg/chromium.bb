// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/power/battery_notification.h"

#include "ash/system/chromeos/power/power_status.h"
#include "ash/system/system_notifier.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"

using message_center::MessageCenter;
using message_center::Notification;

namespace ash {

namespace {

const char kBatteryNotificationId[] = "battery";

gfx::Image& GetBatteryImage(TrayPower::NotificationState notification_state) {
  int resource_id;
  if (PowerStatus::Get()->IsUsbChargerConnected()) {
    resource_id = IDR_AURA_NOTIFICATION_BATTERY_FLUCTUATING;
  } else if (notification_state == TrayPower::NOTIFICATION_LOW_POWER) {
    resource_id = IDR_AURA_NOTIFICATION_BATTERY_LOW;
  } else if (notification_state == TrayPower::NOTIFICATION_CRITICAL) {
    resource_id = IDR_AURA_NOTIFICATION_BATTERY_CRITICAL;
  } else {
    NOTREACHED();
    resource_id = 0;
  }

  return ui::ResourceBundle::GetSharedInstance().GetImageNamed(resource_id);
}

scoped_ptr<Notification> CreateNotification(
    TrayPower::NotificationState notification_state) {
  const PowerStatus& status = *PowerStatus::Get();

  base::string16 message = l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_BATTERY_PERCENT,
      base::IntToString16(status.GetRoundedBatteryPercent()));

  const base::TimeDelta time = status.IsBatteryCharging()
                                   ? status.GetBatteryTimeToFull()
                                   : status.GetBatteryTimeToEmpty();
  base::string16 time_message;
  if (status.IsUsbChargerConnected()) {
    time_message = l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_BATTERY_CHARGING_UNRELIABLE);
  } else if (PowerStatus::ShouldDisplayBatteryTime(time) &&
             !status.IsBatteryDischargingOnLinePower()) {
    int hour = 0, min = 0;
    PowerStatus::SplitTimeIntoHoursAndMinutes(time, &hour, &min);
    if (status.IsBatteryCharging()) {
      time_message = l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_BATTERY_TIME_UNTIL_FULL,
          base::IntToString16(hour), base::IntToString16(min));
    } else {
      // This is a low battery warning prompting the user in minutes.
      time_message = ui::TimeFormat::Simple(
          ui::TimeFormat::FORMAT_REMAINING, ui::TimeFormat::LENGTH_LONG,
          base::TimeDelta::FromMinutes(hour * 60 + min));
    }
  }

  if (!time_message.empty())
    message = message + base::ASCIIToUTF16("\n") + time_message;

  scoped_ptr<Notification> notification(new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, kBatteryNotificationId,
      base::string16(), message, GetBatteryImage(notification_state),
      base::string16(),
      message_center::NotifierId(message_center::NotifierId::SYSTEM_COMPONENT,
                                 system_notifier::kNotifierBattery),
      message_center::RichNotificationData(), NULL));
  notification->SetSystemPriority();
  return notification;
}

}  // namespace

BatteryNotification::BatteryNotification(
    MessageCenter* message_center,
    TrayPower::NotificationState notification_state)
    : message_center_(message_center) {
  message_center_->AddNotification(
      CreateNotification(notification_state).Pass());
}

BatteryNotification::~BatteryNotification() {
  if (message_center_->FindVisibleNotificationById(kBatteryNotificationId))
    message_center_->RemoveNotification(kBatteryNotificationId, false);
}

void BatteryNotification::Update(
    TrayPower::NotificationState notification_state) {
  if (message_center_->FindVisibleNotificationById(kBatteryNotificationId)) {
    message_center_->UpdateNotification(
        kBatteryNotificationId, CreateNotification(notification_state).Pass());
  }
}

}  // namespace ash
