// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/ui/low_disk_notification.h"

#include <stdint.h>

#include "ash/common/system/system_notifier.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_types.h"
#include "ui/message_center/notifier_settings.h"

namespace {

const char kLowDiskId[] = "low_disk";
const char kStoragePage[] = "storage";
const uint64_t kNotificationThreshold = 1 << 30;          // 1GB
const uint64_t kNotificationSevereThreshold = 512 << 20;  // 512MB
constexpr base::TimeDelta kNotificationInterval =
    base::TimeDelta::FromMinutes(2);

class LowDiskNotificationDelegate
    : public message_center::NotificationDelegate {
 public:
  LowDiskNotificationDelegate() {}

  // message_center::NotificationDelegate
  void ButtonClick(int button_index) override {
    chrome::ShowSettingsSubPageForProfile(
        ProfileManager::GetActiveUserProfile(), kStoragePage);
  }

 private:
  ~LowDiskNotificationDelegate() override {}

  DISALLOW_COPY_AND_ASSIGN(LowDiskNotificationDelegate);
};

}  // namespace

namespace chromeos {

LowDiskNotification::LowDiskNotification()
    : message_center_(g_browser_process->message_center()),
      notification_interval_(kNotificationInterval),
      weak_ptr_factory_(this) {
  DCHECK(DBusThreadManager::Get()->GetCryptohomeClient());
  DBusThreadManager::Get()->GetCryptohomeClient()->SetLowDiskSpaceHandler(
      base::Bind(&LowDiskNotification::OnLowDiskSpace,
                 weak_ptr_factory_.GetWeakPtr()));
}

LowDiskNotification::~LowDiskNotification() {
  DCHECK(DBusThreadManager::Get()->GetCryptohomeClient());
  DBusThreadManager::Get()->GetCryptohomeClient()->SetLowDiskSpaceHandler(
      CryptohomeClient::LowDiskSpaceHandler());
}

void LowDiskNotification::OnLowDiskSpace(uint64_t free_disk_bytes) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // We suppress the low-space notifications when there are multiple users on an
  // enterprise managed device. crbug.com/656788.
  if (g_browser_process->platform_part()
          ->browser_policy_connector_chromeos()
          ->IsEnterpriseManaged() &&
      user_manager::UserManager::Get()->GetUsers().size() > 1) {
    LOG(WARNING) << "Device is low on disk space, but the notification was "
                 << "suppressed on a managed device.";
    return;
  }
  Severity severity = GetSeverity(free_disk_bytes);
  base::Time now = base::Time::Now();
  if (severity != last_notification_severity_ ||
      (severity == HIGH &&
       now - last_notification_time_ > notification_interval_)) {
    message_center_->AddNotification(CreateNotification(severity));
    last_notification_time_ = now;
    last_notification_severity_ = severity;
  }
}

std::unique_ptr<message_center::Notification>
LowDiskNotification::CreateNotification(Severity severity) {
  base::string16 title;
  base::string16 message;
  gfx::Image icon;
  switch (severity) {
    case HIGH:
      title =
          l10n_util::GetStringUTF16(IDS_CRITICALLY_LOW_DISK_NOTIFICATION_TITLE);
      message = l10n_util::GetStringUTF16(
          IDS_CRITICALLY_LOW_DISK_NOTIFICATION_MESSAGE);
      icon = gfx::Image(ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_DISK_SPACE_NOTIFICATION_CRITICAL));
      break;
    default:
      title = l10n_util::GetStringUTF16(IDS_LOW_DISK_NOTIFICATION_TITLE);
      message = l10n_util::GetStringUTF16(IDS_LOW_DISK_NOTIFICATION_MESSAGE);
      icon = gfx::Image(ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_DISK_SPACE_NOTIFICATION_LOW));
  }

  message_center::ButtonInfo storage_settings(
      l10n_util::GetStringUTF16(IDS_LOW_DISK_NOTIFICATION_BUTTON));
  storage_settings.icon = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      IDR_STORAGE_MANAGER_BUTTON);
  message_center::RichNotificationData optional_fields;
  optional_fields.buttons.push_back(storage_settings);

  message_center::NotifierId notifier_id(
      message_center::NotifierId::SYSTEM_COMPONENT,
      ash::system_notifier::kNotifierDisk);

  std::unique_ptr<message_center::Notification> notification(
      new message_center::Notification(
          message_center::NOTIFICATION_TYPE_SIMPLE, kLowDiskId, title, message,
          icon, base::string16(), GURL(), notifier_id, optional_fields,
          new LowDiskNotificationDelegate()));

  return notification;
}

LowDiskNotification::Severity LowDiskNotification::GetSeverity(
    uint64_t free_disk_bytes) {
  if (free_disk_bytes < kNotificationSevereThreshold)
    return Severity::HIGH;
  if (free_disk_bytes < kNotificationThreshold)
    return Severity::MEDIUM;
  return Severity::NONE;
}

void LowDiskNotification::SetMessageCenterForTest(
    message_center::MessageCenter* message_center) {
  message_center_ = message_center;
}

void LowDiskNotification::SetNotificationIntervalForTest(
    base::TimeDelta notification_interval) {
  notification_interval_ = notification_interval;
}

}  // namespace chromeos
