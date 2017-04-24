// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_migration_guide_notification.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/known_user.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_delegate.h"

namespace arc {

namespace {

constexpr char kNotifierId[] = "arc_fs_migration";
constexpr char kSuggestNotificationId[] = "arc_fs_migration/suggest";
constexpr char kSuccessNotificationId[] = "arc_fs_migration/success";

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

  message_center::RichNotificationData data;
  data.buttons.push_back(message_center::ButtonInfo(l10n_util::GetStringUTF16(
      IDS_ARC_MIGRATE_ENCRYPTION_NOTIFICATION_RESTART_BUTTON)));
  message_center::MessageCenter::Get()->AddNotification(
      base::MakeUnique<message_center::Notification>(
          message_center::NOTIFICATION_TYPE_SIMPLE, kSuggestNotificationId,
          l10n_util::GetStringUTF16(
              IDS_ARC_MIGRATE_ENCRYPTION_NOTIFICATION_TITLE),
          // TODO(kinaba): crbug/710289 Change message for low-battery case.
          l10n_util::GetStringUTF16(
              IDS_ARC_MIGRATE_ENCRYPTION_NOTIFICATION_MESSAGE),
          gfx::Image(gfx::CreateVectorIcon(
              kArcMigrateEncryptionNotificationIcon, gfx::kPlaceholderColor)),
          base::string16(), GURL(), notifier_id, data,
          new ArcMigrationGuideNotificationDelegate()));
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

  // If this is a newly created profile, the filesystem was compatible from
  // the beginning, not because of migration. Skip showing the notification.
  if (!profile->IsNewProfile()) {
    message_center::NotifierId notifier_id(
        message_center::NotifierId::SYSTEM_COMPONENT, kNotifierId);
    notifier_id.profile_id = account_id.GetUserEmail();

    message_center::MessageCenter::Get()->AddNotification(
        base::MakeUnique<message_center::Notification>(
            message_center::NOTIFICATION_TYPE_SIMPLE, kSuccessNotificationId,
            base::string16(),  // title
            l10n_util::GetStringUTF16(
                IDS_ARC_MIGRATE_ENCRYPTION_SUCCESS_MESSAGE),
            gfx::Image(gfx::CreateVectorIcon(
                kArcMigrateEncryptionNotificationIcon, gfx::kPlaceholderColor)),
            base::string16(), GURL(), notifier_id,
            message_center::RichNotificationData(),
            new message_center::NotificationDelegate()));
  }

  // Mark as notified.
  user_manager::known_user::SetIntegerPref(
      account_id, prefs::kArcCompatibleFilesystemChosen,
      arc::kFileSystemCompatibleAndNotified);
}

}  // namespace arc
