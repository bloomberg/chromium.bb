// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/easy_unlock_notification_controller_chromeos.h"

#include "ash/system/devicetype_utils.h"
#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/proximity_auth/screenlock_bridge.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/message_center_types.h"
#include "ui/message_center/notification_types.h"

namespace {

const char kEasyUnlockChromebookAddedNotifierId[] =
    "easyunlock_notification_ids.chromebook_added";

const char kEasyUnlockPairingChangeNotifierId[] =
    "easyunlock_notification_ids.pairing_change";

const char kEasyUnlockPairingChangeAppliedNotifierId[] =
    "easyunlock_notification_ids.pairing_change_applied";

const char kEasyUnlockPromotionNotifierId[] =
    "easyunlock_notification_ids.promotion";

const char kLockScreenSettingsSubpage[] = "lockScreen";

// Convenience function for creating a Notification.
std::unique_ptr<message_center::Notification> CreateNotification(
    const std::string& id,
    const base::string16& title,
    const base::string16& message,
    const gfx::Image& icon,
    const message_center::RichNotificationData& rich_notification_data,
    message_center::NotificationDelegate* delegate) {
  return base::MakeUnique<message_center::Notification>(
      message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE, id, title,
      message, icon, base::string16() /* display_source */,
      GURL() /* origin_url */,
      message_center::NotifierId(
          message_center::NotifierId::NotifierType::SYSTEM_COMPONENT, id),
      rich_notification_data, delegate);
}

}  // namespace

EasyUnlockNotificationControllerChromeOS::
    EasyUnlockNotificationControllerChromeOS(
        Profile* profile,
        message_center::MessageCenter* message_center)
    : profile_(profile),
      message_center_(message_center),
      weak_ptr_factory_(this) {}

EasyUnlockNotificationControllerChromeOS::
    ~EasyUnlockNotificationControllerChromeOS() {}

void EasyUnlockNotificationControllerChromeOS::
    ShowChromebookAddedNotification() {
  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.buttons.push_back(
      message_center::ButtonInfo(l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_CHROMEBOOK_ADDED_NOTIFICATION_ABOUT_BUTTON)));

  ShowNotification(CreateNotification(
      kEasyUnlockChromebookAddedNotifierId,
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_CHROMEBOOK_ADDED_NOTIFICATION_TITLE),
      l10n_util::GetStringFUTF16(
          IDS_EASY_UNLOCK_CHROMEBOOK_ADDED_NOTIFICATION_MESSAGE,
          ash::GetChromeOSDeviceName()),
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_NOTIFICATION_EASYUNLOCK_ENABLED),
      rich_notification_data,
      new NotificationDelegate(kEasyUnlockChromebookAddedNotifierId,
                               weak_ptr_factory_.GetWeakPtr())));
}

void EasyUnlockNotificationControllerChromeOS::ShowPairingChangeNotification() {
  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.buttons.push_back(
      message_center::ButtonInfo(l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_PAIRING_CHANGED_NOTIFICATION_UPDATE_BUTTON)));
  rich_notification_data.buttons.push_back(
      message_center::ButtonInfo(l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_CHROMEBOOK_ADDED_NOTIFICATION_ABOUT_BUTTON)));

  ShowNotification(CreateNotification(
      kEasyUnlockPairingChangeNotifierId,
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_PAIRING_CHANGED_NOTIFICATION_TITLE),
      l10n_util::GetStringFUTF16(
          IDS_EASY_UNLOCK_PAIRING_CHANGED_NOTIFICATION_MESSAGE,
          ash::GetChromeOSDeviceName()),
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_NOTIFICATION_EASYUNLOCK_ENABLED),
      rich_notification_data,
      new NotificationDelegate(kEasyUnlockPairingChangeNotifierId,
                               weak_ptr_factory_.GetWeakPtr())));
}

void EasyUnlockNotificationControllerChromeOS::
    ShowPairingChangeAppliedNotification(const std::string& phone_name) {
  // Remove the pairing change notification if it is still being shown.
  message_center_->RemoveNotification(kEasyUnlockPairingChangeNotifierId,
                                      false /* by_user */);

  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.buttons.push_back(
      message_center::ButtonInfo(l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_CHROMEBOOK_ADDED_NOTIFICATION_ABOUT_BUTTON)));

  ShowNotification(CreateNotification(
      kEasyUnlockPairingChangeAppliedNotifierId,
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_PAIRING_CHANGE_APPLIED_NOTIFICATION_TITLE),
      l10n_util::GetStringFUTF16(
          IDS_EASY_UNLOCK_PAIRING_CHANGE_APPLIED_NOTIFICATION_MESSAGE,
          base::UTF8ToUTF16(phone_name), ash::GetChromeOSDeviceName()),
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_NOTIFICATION_EASYUNLOCK_ENABLED),
      rich_notification_data,
      new NotificationDelegate(kEasyUnlockPairingChangeAppliedNotifierId,
                               weak_ptr_factory_.GetWeakPtr())));
}

void EasyUnlockNotificationControllerChromeOS::ShowPromotionNotification() {
  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.buttons.push_back(
      message_center::ButtonInfo(l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SETUP_NOTIFICATION_BUTTON_TITLE)));

  ShowNotification(CreateNotification(
      kEasyUnlockPromotionNotifierId,
      l10n_util::GetStringFUTF16(IDS_EASY_UNLOCK_SETUP_NOTIFICATION_TITLE,
                                 ash::GetChromeOSDeviceName()),
      l10n_util::GetStringFUTF16(IDS_EASY_UNLOCK_SETUP_NOTIFICATION_MESSAGE,
                                 ash::GetChromeOSDeviceName()),
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_NOTIFICATION_EASYUNLOCK_PROMO),
      rich_notification_data,
      new NotificationDelegate(kEasyUnlockPromotionNotifierId,
                               weak_ptr_factory_.GetWeakPtr())));
}

void EasyUnlockNotificationControllerChromeOS::ShowNotification(
    std::unique_ptr<message_center::Notification> notification) {
  notification->SetSystemPriority();
  std::string notification_id = notification->id();
  if (message_center_->FindVisibleNotificationById(notification_id)) {
    message_center_->UpdateNotification(notification_id,
                                        std::move(notification));
  } else {
    message_center_->AddNotification(std::move(notification));
  }
}

void EasyUnlockNotificationControllerChromeOS::LaunchEasyUnlockSettings() {
  chrome::ShowSettingsSubPageForProfile(profile_, kLockScreenSettingsSubpage);
}

void EasyUnlockNotificationControllerChromeOS::LockScreen() {
  proximity_auth::ScreenlockBridge::Get()->Lock();
}

EasyUnlockNotificationControllerChromeOS::NotificationDelegate::
    NotificationDelegate(
        const std::string& notification_id,
        const base::WeakPtr<EasyUnlockNotificationControllerChromeOS>&
            notification_controller)
    : notification_id_(notification_id),
      notification_controller_(notification_controller) {}

EasyUnlockNotificationControllerChromeOS::NotificationDelegate::
    ~NotificationDelegate() {}

void EasyUnlockNotificationControllerChromeOS::NotificationDelegate::Click() {
  if (!notification_controller_)
    return;

  if (notification_id_ == kEasyUnlockChromebookAddedNotifierId ||
      notification_id_ == kEasyUnlockPairingChangeAppliedNotifierId ||
      notification_id_ == kEasyUnlockPromotionNotifierId)
    notification_controller_->LaunchEasyUnlockSettings();
}

void EasyUnlockNotificationControllerChromeOS::NotificationDelegate::
    ButtonClick(int button_index) {
  if (!notification_controller_)
    return;

  if (notification_id_ == kEasyUnlockChromebookAddedNotifierId ||
      notification_id_ == kEasyUnlockPairingChangeAppliedNotifierId ||
      notification_id_ == kEasyUnlockPromotionNotifierId) {
    notification_controller_->LaunchEasyUnlockSettings();
  } else if (notification_id_ == kEasyUnlockPairingChangeNotifierId) {
    if (button_index == 0) {
      notification_controller_->LockScreen();
    } else if (button_index == 1) {
      notification_controller_->LaunchEasyUnlockSettings();
    }
  }
}
