// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/managed/tray_locally_managed_user.h"

#include "ash/shell.h"
#include "ash/system/chromeos/label_tray_view.h"
#include "ash/system/system_notifier.h"
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

const char TrayLocallyManagedUser::kNotificationId[] =
    "chrome://user/locally-managed";

TrayLocallyManagedUser::TrayLocallyManagedUser(SystemTray* system_tray)
    : SystemTrayItem(system_tray),
      tray_view_(NULL),
      status_(ash::user::LOGGED_IN_NONE) {
}

TrayLocallyManagedUser::~TrayLocallyManagedUser() {
}

void TrayLocallyManagedUser::UpdateMessage() {
  base::string16 message = Shell::GetInstance()->system_tray_delegate()->
      GetLocallyManagedUserMessage();
  if (tray_view_)
    tray_view_->SetMessage(message);
  if (message_center::MessageCenter::Get()->HasNotification(kNotificationId))
    CreateOrUpdateNotification(message);
}

views::View* TrayLocallyManagedUser::CreateDefaultView(
    user::LoginStatus status) {
  CHECK(tray_view_ == NULL);
  if (status != ash::user::LOGGED_IN_LOCALLY_MANAGED)
    return NULL;

  tray_view_ = new LabelTrayView(this, IDR_AURA_UBER_TRAY_MANAGED_USER);
  UpdateMessage();
  return tray_view_;
}

void TrayLocallyManagedUser::DestroyDefaultView() {
  tray_view_ = NULL;
}

void TrayLocallyManagedUser::OnViewClicked(views::View* sender) {
  Shell::GetInstance()->system_tray_delegate()->ShowLocallyManagedUserInfo();
}

void TrayLocallyManagedUser::UpdateAfterLoginStatusChange(
    user::LoginStatus status) {
  if (status == status_)
    return;
  if (status == ash::user::LOGGED_IN_LOCALLY_MANAGED &&
      status_ != ash::user::LOGGED_IN_LOCKED) {
    SystemTrayDelegate* delegate = Shell::GetInstance()->system_tray_delegate();
    CreateOrUpdateNotification(delegate->GetLocallyManagedUserMessage());
  }
  status_ = status;
}

void TrayLocallyManagedUser::CreateOrUpdateNotification(
    const base::string16& new_message) {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  scoped_ptr<Notification> notification(
      message_center::Notification::CreateSystemNotification(
          kNotificationId,
          base::string16() /* no title */,
          new_message,
          bundle.GetImageNamed(IDR_AURA_UBER_TRAY_MANAGED_USER),
          system_notifier::kNotifierLocallyManagedUser,
          base::Closure() /* null callback */));
  message_center::MessageCenter::Get()->AddNotification(notification.Pass());
}

}  // namespace ash
