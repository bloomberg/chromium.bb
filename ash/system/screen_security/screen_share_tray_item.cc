// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/screen_security/screen_share_tray_item.h"

#include <utility>

#include "ash/public/cpp/ash_features.h"
#include "ash/resources/grit/ash_resources.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

using message_center::MessageCenter;
using message_center::Notification;

namespace ash {
namespace {

const char kScreenShareNotificationId[] = "chrome://screen/share";
const char kNotifierScreenShare[] = "ash.screen-share";
}

ScreenShareTrayItem::ScreenShareTrayItem(SystemTray* system_tray)
    : ScreenTrayItem(system_tray, UMA_SCREEN_SHARE) {
  Shell::Get()->system_tray_notifier()->AddScreenShareObserver(this);
}

ScreenShareTrayItem::~ScreenShareTrayItem() {
  Shell::Get()->system_tray_notifier()->RemoveScreenShareObserver(this);
}

views::View* ScreenShareTrayItem::CreateDefaultView(LoginStatus status) {
  set_default_view(new tray::ScreenStatusView(
      this,
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SCREEN_SHARE_BEING_HELPED),
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SCREEN_SHARE_STOP)));
  return default_view();
}

void ScreenShareTrayItem::CreateOrUpdateNotification() {
  base::string16 help_label_text;
  if (!helper_name_.empty()) {
    help_label_text = l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_SCREEN_SHARE_BEING_HELPED_NAME, helper_name_);
  } else {
    help_label_text = l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_SCREEN_SHARE_BEING_HELPED);
  }

  message_center::RichNotificationData data;
  data.buttons.push_back(message_center::ButtonInfo(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SCREEN_SHARE_STOP)));
  std::unique_ptr<Notification> notification =
      Notification::CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE, kScreenShareNotificationId,
          l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SCREEN_SHARE_TITLE),
          help_label_text, gfx::Image(), base::string16() /* display_source */,
          GURL(),
          message_center::NotifierId(
              message_center::NotifierId::SYSTEM_COMPONENT,
              kNotifierScreenShare),
          data, new tray::ScreenNotificationDelegate(this),
          kNotificationScreenshareIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);
  notification->SetSystemPriority();
  if (features::IsSystemTrayUnifiedEnabled())
    notification->set_pinned(true);
  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

std::string ScreenShareTrayItem::GetNotificationId() {
  return kScreenShareNotificationId;
}

void ScreenShareTrayItem::RecordStoppedFromDefaultViewMetric() {
  // Intentionally not recording a metric.
}

void ScreenShareTrayItem::RecordStoppedFromNotificationViewMetric() {
  // Intentionally not recording a metric.
}

void ScreenShareTrayItem::OnScreenShareStart(
    const base::Closure& stop_callback,
    const base::string16& helper_name) {
  helper_name_ = helper_name;
  Start(stop_callback);
}

void ScreenShareTrayItem::OnScreenShareStop() {
  // We do not need to run the stop callback
  // when screening is stopped externally.
  set_is_started(false);
  Update();
}

}  // namespace ash
