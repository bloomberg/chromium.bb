// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_auth_notification.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/common/system/chromeos/devicetype_utils.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/arc/arc_optin_uma.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/signin/core/account_id/account_id.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_delegate.h"
#include "url/gurl.h"

namespace {

bool g_disabled = false;

// Ids of the notification shown on first run.
const char kNotifierId[] = "arc_auth";
const char kDisplaySource[] = "arc_auth_source";
const char kFirstRunNotificationId[] = "arc_auth/first_run";

class ArcAuthNotificationDelegate
    : public message_center::NotificationDelegate,
      public message_center::MessageCenterObserver {
 public:
  explicit ArcAuthNotificationDelegate(Profile* profile) : profile_(profile) {}

  // message_center::MessageCenterObserver
  void OnNotificationUpdated(const std::string& notification_id) override {
    if (notification_id != kFirstRunNotificationId)
      return;

    StopObserving();
    message_center::Notification* notification =
        message_center::MessageCenter::Get()->FindVisibleNotificationById(
            notification_id);
    if (!notification) {
      NOTREACHED();
      return;
    }

    if (!notification->IsRead())
      UpdateOptInActionUMA(arc::OptInActionType::NOTIFICATION_TIMED_OUT);
  }

  // message_center::NotificationDelegate
  void Display() override { StartObserving(); }

  void ButtonClick(int button_index) override {
    StopObserving();
    if (button_index == 0) {
      UpdateOptInActionUMA(arc::OptInActionType::NOTIFICATION_ACCEPTED);
      arc::SetArcPlayStoreEnabledForProfile(profile_, true);
    } else {
      UpdateOptInActionUMA(arc::OptInActionType::NOTIFICATION_DECLINED);
      arc::SetArcPlayStoreEnabledForProfile(profile_, false);
    }
  }

  void Close(bool by_user) override { StopObserving(); }

 private:
  ~ArcAuthNotificationDelegate() override { StopObserving(); }

  void StartObserving() {
    message_center::MessageCenter::Get()->AddObserver(this);
  }

  void StopObserving() {
    message_center::MessageCenter::Get()->RemoveObserver(this);
  }

  Profile* const profile_;

  DISALLOW_COPY_AND_ASSIGN(ArcAuthNotificationDelegate);
};

}  // namespace

namespace arc {

// static
void ArcAuthNotification::Show(Profile* profile) {
  if (g_disabled)
    return;

  message_center::NotifierId notifier_id(
      message_center::NotifierId::SYSTEM_COMPONENT, kNotifierId);
  notifier_id.profile_id =
      multi_user_util::GetAccountIdFromProfile(profile).GetUserEmail();

  message_center::RichNotificationData data;
  data.buttons.push_back(message_center::ButtonInfo(
      l10n_util::GetStringUTF16(IDS_ARC_OPEN_PLAY_STORE_NOTIFICATION_BUTTON)));
  data.buttons.push_back(message_center::ButtonInfo(
      l10n_util::GetStringUTF16(IDS_ARC_CANCEL_NOTIFICATION_BUTTON)));
  ui::ResourceBundle& resource_bundle = ui::ResourceBundle::GetSharedInstance();
  std::unique_ptr<message_center::Notification> notification(
      new message_center::Notification(
          message_center::NOTIFICATION_TYPE_SIMPLE, kFirstRunNotificationId,
          l10n_util::GetStringUTF16(IDS_ARC_NOTIFICATION_TITLE),
          l10n_util::GetStringFUTF16(IDS_ARC_NOTIFICATION_MESSAGE,
                                     ash::GetChromeOSDeviceName()),
          resource_bundle.GetImageNamed(IDR_ARC_PLAY_STORE_NOTIFICATION),
          base::UTF8ToUTF16(kDisplaySource), GURL(), notifier_id, data,
          new ArcAuthNotificationDelegate(profile)));
  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

// static
void ArcAuthNotification::Hide() {
  if (g_disabled)
    return;

  message_center::MessageCenter::Get()->RemoveNotification(
      kFirstRunNotificationId, false);
}

// static
void ArcAuthNotification::DisableForTesting() {
  g_disabled = true;
}

}  // namespace arc
