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
      status_(ash::user::LOGGED_IN_NONE),
      is_user_supervised_(false) {
  Shell::GetInstance()->system_tray_delegate()->
      AddCustodianInfoTrayObserver(this);
}

TraySupervisedUser::~TraySupervisedUser() {
  // We need the check as on shell destruction delegate is destroyed first.
  SystemTrayDelegate* system_tray_delegate =
      Shell::GetInstance()->system_tray_delegate();
  if (system_tray_delegate)
    system_tray_delegate->RemoveCustodianInfoTrayObserver(this);
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
  SystemTrayDelegate* delegate = Shell::GetInstance()->system_tray_delegate();
  if (!delegate->IsUserSupervised())
    return NULL;

  tray_view_ = new LabelTrayView(this, GetSupervisedUserIconId());
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
  SystemTrayDelegate* delegate = Shell::GetInstance()->system_tray_delegate();

  bool is_user_supervised = delegate->IsUserSupervised();
  if (status == status_ && is_user_supervised == is_user_supervised_)
    return;

  if (is_user_supervised &&
      !delegate->IsUserChild() &&
      status_ != ash::user::LOGGED_IN_LOCKED &&
      !delegate->GetSupervisedUserManager().empty())
    CreateOrUpdateSupervisedWarningNotification();

  status_ = status;
  is_user_supervised_ = is_user_supervised;
}

void TraySupervisedUser::CreateOrUpdateNotification(
    const base::string16& new_message) {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  scoped_ptr<Notification> notification(
      message_center::Notification::CreateSystemNotification(
          kNotificationId, base::string16() /* no title */, new_message,
          bundle.GetImageNamed(GetSupervisedUserIconId()),
          system_notifier::kNotifierSupervisedUser,
          base::Closure() /* null callback */));
  message_center::MessageCenter::Get()->AddNotification(notification.Pass());
}

void TraySupervisedUser::CreateOrUpdateSupervisedWarningNotification() {
  SystemTrayDelegate* delegate = Shell::GetInstance()->system_tray_delegate();
  CreateOrUpdateNotification(delegate->GetSupervisedUserMessage());
}

void TraySupervisedUser::OnCustodianInfoChanged() {
  SystemTrayDelegate* delegate = Shell::GetInstance()->system_tray_delegate();
  std::string manager_name = delegate->GetSupervisedUserManager();
  if (!manager_name.empty()) {
    if (!delegate->IsUserChild() &&
        !message_center::MessageCenter::Get()->FindVisibleNotificationById(
            kNotificationId)) {
      CreateOrUpdateSupervisedWarningNotification();
    }
    UpdateMessage();
  }
}

int TraySupervisedUser::GetSupervisedUserIconId() const {
  SystemTrayDelegate* delegate = Shell::GetInstance()->system_tray_delegate();

  // Not intended to be used for non-supervised users.
  CHECK(delegate->IsUserSupervised());

  if (delegate->IsUserChild())
    return IDR_AURA_UBER_TRAY_CHILD_USER;
  return IDR_AURA_UBER_TRAY_SUPERVISED_USER;
}

}  // namespace ash
