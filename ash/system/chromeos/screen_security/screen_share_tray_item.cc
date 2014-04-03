// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/screen_security/screen_share_tray_item.h"

#include "ash/shell.h"
#include "ash/system/system_notifier.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"

using message_center::Notification;

namespace ash {
namespace {

const char kScreenShareNotificationId[] = "chrome://screen/share";

}

ScreenShareTrayItem::ScreenShareTrayItem(SystemTray* system_tray)
    : ScreenTrayItem(system_tray) {
  Shell::GetInstance()->system_tray_notifier()->
      AddScreenShareObserver(this);
}

ScreenShareTrayItem::~ScreenShareTrayItem() {
  Shell::GetInstance()->system_tray_notifier()->
      RemoveScreenShareObserver(this);
}

views::View* ScreenShareTrayItem::CreateTrayView(user::LoginStatus status) {
  set_tray_view(
      new tray::ScreenTrayView(this, IDR_AURA_UBER_TRAY_SCREENSHARE));
  return tray_view();
}

views::View* ScreenShareTrayItem::CreateDefaultView(user::LoginStatus status) {
  set_default_view(new tray::ScreenStatusView(
      this,
      IDR_AURA_UBER_TRAY_SCREENSHARE_DARK,
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_SCREEN_SHARE_BEING_HELPED),
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_SCREEN_SHARE_STOP)));
  return default_view();
}

void ScreenShareTrayItem::CreateOrUpdateNotification() {
  base::string16 help_label_text;
  if (!helper_name_.empty()) {
    help_label_text = l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_SCREEN_SHARE_BEING_HELPED_NAME,
        helper_name_);
  } else {
    help_label_text = l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_SCREEN_SHARE_BEING_HELPED);
  }

  message_center::RichNotificationData data;
  data.buttons.push_back(message_center::ButtonInfo(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SCREEN_SHARE_STOP)));
  ui::ResourceBundle& resource_bundle = ui::ResourceBundle::GetSharedInstance();
  scoped_ptr<Notification> notification(new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      kScreenShareNotificationId,
      help_label_text,
      base::string16() /* body is blank */,
      resource_bundle.GetImageNamed(IDR_AURA_UBER_TRAY_SCREENSHARE_DARK),
      base::string16() /* display_source */,
      message_center::NotifierId(
          message_center::NotifierId::SYSTEM_COMPONENT,
          system_notifier::kNotifierScreenShare),
      data,
      new tray::ScreenNotificationDelegate(this)));
  notification->SetSystemPriority();
  message_center::MessageCenter::Get()->AddNotification(notification.Pass());
}

std::string ScreenShareTrayItem::GetNotificationId() {
  return kScreenShareNotificationId;
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
