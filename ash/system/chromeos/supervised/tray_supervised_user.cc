// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/supervised/tray_supervised_user.h"

#include "ash/shell.h"
#include "ash/system/chromeos/label_tray_view.h"
#include "ash/system/system_notifier.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_notification_view.h"
#include "ash/system/user/login_status.h"
#include "base/callback.h"
#include "base/logging.h"
#include "grit/ash_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_delegate.h"

using message_center::Notification;

namespace ash {

const char TraySupervisedUser::kNotificationId[] =
    "chrome://user/locally-managed";

TraySupervisedUser::TraySupervisedUser(SystemTray* system_tray)
    : SystemTrayItem(system_tray),
      tray_view_(NULL),
      status_(ash::user::LOGGED_IN_NONE) {
}

TraySupervisedUser::~TraySupervisedUser() {
}

void TraySupervisedUser::UpdateMessage() {
  base::string16 message = Shell::GetInstance()->system_tray_delegate()->
      GetSupervisedUserMessage();
  if (tray_view_)
    tray_view_->SetMessage(message);
  if (message_center::MessageCenter::Get()->FindVisibleNotificationById(
      kNotificationId))
    CreateOrUpdateNotification(message);
}

views::View* TraySupervisedUser::CreateDefaultView(
    user::LoginStatus status) {
  CHECK(tray_view_ == NULL);
  if (status != ash::user::LOGGED_IN_SUPERVISED)
    return NULL;

  tray_view_ = new LabelTrayView(this, IDR_AURA_UBER_TRAY_SUPERVISED_USER);
  UpdateMessage();
  return tray_view_;
}

void TraySupervisedUser::DestroyDefaultView() {
  tray_view_ = NULL;
}

void TraySupervisedUser::OnViewClicked(views::View* sender) {
  Shell::GetInstance()->system_tray_delegate()->ShowSupervisedUserInfo();
}

void TraySupervisedUser::UpdateAfterLoginStatusChange(
    user::LoginStatus status) {
  if (status == status_)
    return;
  if (status == ash::user::LOGGED_IN_SUPERVISED &&
      status_ != ash::user::LOGGED_IN_LOCKED) {
    SystemTrayDelegate* delegate = Shell::GetInstance()->system_tray_delegate();
    CreateOrUpdateNotification(delegate->GetSupervisedUserMessage());
  }
  status_ = status;
}

void TraySupervisedUser::CreateOrUpdateNotification(
    const base::string16& new_message) {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  scoped_ptr<Notification> notification(
      message_center::Notification::CreateSystemNotification(
          kNotificationId,
          base::string16() /* no title */,
          new_message,
          bundle.GetImageNamed(IDR_AURA_UBER_TRAY_SUPERVISED_USER),
          system_notifier::kNotifierSupervisedUser,
          base::Closure() /* null callback */));
  message_center::MessageCenter::Get()->AddNotification(notification.Pass());
}

}  // namespace ash
