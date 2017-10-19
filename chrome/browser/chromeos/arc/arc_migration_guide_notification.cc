// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_migration_guide_notification.h"

#include <memory>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/system/power/power_status.h"
#include "base/macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/chromeos/arc/arc_migration_constants.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/arc/arc_prefs.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/known_user.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_delegate.h"
#include "ui/message_center/public/cpp/message_center_switches.h"

namespace arc {

namespace {

constexpr char kNotifierId[] = "arc_fs_migration";
constexpr char kSuggestNotificationId[] = "arc_fs_migration/suggest";
constexpr char kSuccessNotificationId[] = "arc_fs_migration/success";
constexpr base::TimeDelta kSuccessNotificationDelay =
    base::TimeDelta::FromSeconds(3);

class ArcMigrationGuideNotificationDelegate
    : public message_center::NotificationDelegate {
 public:
  ArcMigrationGuideNotificationDelegate() = default;

  // message_center::NotificationDelegate
  void ButtonClick(int button_index) override { chrome::AttemptUserExit(); }

 private:
  ~ArcMigrationGuideNotificationDelegate() override = default;

  DISALLOW_COPY_AND_ASSIGN(ArcMigrationGuideNotificationDelegate);
};

void DoShowArcMigrationSuccessNotification(
    const message_center::NotifierId& notifier_id) {
  if (message_center::IsNewStyleNotificationEnabled()) {
    message_center::MessageCenter::Get()->AddNotification(
        message_center::Notification::CreateSystemNotification(
            message_center::NOTIFICATION_TYPE_SIMPLE, kSuccessNotificationId,
            l10n_util::GetStringUTF16(IDS_ARC_MIGRATE_ENCRYPTION_SUCCESS_TITLE),
            l10n_util::GetStringUTF16(
                IDS_ARC_MIGRATE_ENCRYPTION_SUCCESS_MESSAGE),
            gfx::Image(), base::string16(), GURL(), notifier_id,
            message_center::RichNotificationData(),
            new message_center::NotificationDelegate(),
            ash::kNotificationSettingsIcon,
            message_center::SystemNotificationWarningLevel::NORMAL));
  } else {
    message_center::MessageCenter::Get()->AddNotification(
        std::make_unique<message_center::Notification>(
            message_center::NOTIFICATION_TYPE_SIMPLE, kSuccessNotificationId,
            l10n_util::GetStringUTF16(IDS_ARC_MIGRATE_ENCRYPTION_SUCCESS_TITLE),
            l10n_util::GetStringUTF16(
                IDS_ARC_MIGRATE_ENCRYPTION_SUCCESS_MESSAGE),
            gfx::Image(gfx::CreateVectorIcon(
                kArcMigrateEncryptionNotificationIcon, gfx::kPlaceholderColor)),
            base::string16(), GURL(), notifier_id,
            message_center::RichNotificationData(),
            new message_center::NotificationDelegate()));
  }
}

}  // namespace

// static
void ShowArcMigrationGuideNotification(Profile* profile) {
  // Always remove the notification to make sure the notification appears
  // as a popup in any situation.
  message_center::MessageCenter::Get()->RemoveNotification(
      kSuggestNotificationId, false /* by_user */);

  message_center::NotifierId notifier_id(
      message_center::NotifierId::SYSTEM_COMPONENT, kNotifierId);
  notifier_id.profile_id =
      multi_user_util::GetAccountIdFromProfile(profile).GetUserEmail();

  const bool is_low_battery = ash::PowerStatus::IsInitialized() &&
                              ash::PowerStatus::Get()->IsBatteryPresent() &&
                              ash::PowerStatus::Get()->GetBatteryPercent() <
                                  kMigrationMinimumBatteryPercent;

  const base::string16 message =
      is_low_battery
          ? ui::SubstituteChromeOSDeviceType(
                IDS_ARC_MIGRATE_ENCRYPTION_NOTIFICATION_LOW_BATTERY_MESSAGE)
          : l10n_util::GetStringUTF16(
                IDS_ARC_MIGRATE_ENCRYPTION_NOTIFICATION_MESSAGE);

  if (message_center::IsNewStyleNotificationEnabled()) {
    message_center::MessageCenter::Get()->AddNotification(
        message_center::Notification::CreateSystemNotification(
            message_center::NOTIFICATION_TYPE_SIMPLE, kSuggestNotificationId,
            l10n_util::GetStringUTF16(
                IDS_ARC_MIGRATE_ENCRYPTION_NOTIFICATION_TITLE),
            message, gfx::Image(), base::string16(), GURL(), notifier_id,
            message_center::RichNotificationData(),
            new ArcMigrationGuideNotificationDelegate(),
            ash::kNotificationSettingsIcon,
            message_center::SystemNotificationWarningLevel::NORMAL));
  } else {
    message_center::RichNotificationData data;
    data.buttons.push_back(message_center::ButtonInfo(l10n_util::GetStringUTF16(
        IDS_ARC_MIGRATE_ENCRYPTION_NOTIFICATION_RESTART_BUTTON)));
    message_center::MessageCenter::Get()->AddNotification(
        std::make_unique<message_center::Notification>(
            message_center::NOTIFICATION_TYPE_SIMPLE, kSuggestNotificationId,
            l10n_util::GetStringUTF16(
                IDS_ARC_MIGRATE_ENCRYPTION_NOTIFICATION_TITLE),
            message,
            gfx::Image(gfx::CreateVectorIcon(
                kArcMigrateEncryptionNotificationIcon, gfx::kPlaceholderColor)),
            base::string16(), GURL(), notifier_id, data,
            new ArcMigrationGuideNotificationDelegate()));
  }
}

void ShowArcMigrationSuccessNotificationIfNeeded(Profile* profile) {
  const AccountId account_id =
      multi_user_util::GetAccountIdFromProfile(profile);

  int pref_value = kFileSystemIncompatible;
  user_manager::known_user::GetIntegerPref(
      account_id, prefs::kArcCompatibleFilesystemChosen, &pref_value);

  // Show notification only when the pref value indicates the file system is
  // compatible, but not yet notified.
  if (pref_value != kFileSystemCompatible)
    return;

  if (profile->IsNewProfile()) {
    // If this is a newly created profile, the filesystem was compatible from
    // the beginning, not because of migration. Skip showing the notification.
  } else if (!arc::IsArcAllowedForProfile(profile)) {
    // TODO(kinaba; crbug.com/721631): the current message mentions ARC,
    // which is inappropriate for users disallowed running ARC.

    // Log a warning message because, for now, this should not basically happen
    // except for some exceptional situation or due to some bug.
    LOG(WARNING) << "Migration has happened for an ARC-disallowed user.";
  } else {
    message_center::NotifierId notifier_id(
        message_center::NotifierId::SYSTEM_COMPONENT, kNotifierId);
    notifier_id.profile_id = account_id.GetUserEmail();

    // Delay the notification to make sure that it is not hidden behind windows
    // which are shown at the beginning of user session (e.g. Chrome).
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DoShowArcMigrationSuccessNotification, notifier_id),
        kSuccessNotificationDelay);
  }

  // Mark as notified.
  user_manager::known_user::SetIntegerPref(
      account_id, prefs::kArcCompatibleFilesystemChosen,
      arc::kFileSystemCompatibleAndNotified);
}

}  // namespace arc
