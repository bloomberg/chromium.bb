// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/supervised/tray_supervised_user.h"

#include <utility>

#include "ash/common/login_status.h"
#include "ash/common/system/system_notifier.h"
#include "ash/common/system/tray/label_tray_view.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/wm_shell.h"
#include "ash/resources/grit/ash_resources.h"
#include "base/callback.h"
#include "base/logging.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_delegate.h"

using message_center::Notification;

namespace ash {

const char TraySupervisedUser::kNotificationId[] =
    "chrome://user/locally-managed";

TraySupervisedUser::TraySupervisedUser(SystemTray* system_tray)
    : SystemTrayItem(system_tray, UMA_SUPERVISED_USER),
      tray_view_(NULL),
      status_(LoginStatus::NOT_LOGGED_IN),
      is_user_supervised_(false) {
  WmShell::Get()->system_tray_delegate()->AddCustodianInfoTrayObserver(this);
}

TraySupervisedUser::~TraySupervisedUser() {
  // We need the check as on shell destruction delegate is destroyed first.
  SystemTrayDelegate* system_tray_delegate =
      WmShell::Get()->system_tray_delegate();
  if (system_tray_delegate)
    system_tray_delegate->RemoveCustodianInfoTrayObserver(this);
}

void TraySupervisedUser::UpdateMessage() {
  base::string16 message =
      WmShell::Get()->system_tray_delegate()->GetSupervisedUserMessage();
  if (tray_view_)
    tray_view_->SetMessage(message);
  if (message_center::MessageCenter::Get()->FindVisibleNotificationById(
          kNotificationId))
    CreateOrUpdateNotification(message);
}

views::View* TraySupervisedUser::CreateDefaultView(LoginStatus status) {
  CHECK(tray_view_ == NULL);
  SystemTrayDelegate* delegate = WmShell::Get()->system_tray_delegate();
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
  // TODO(antrim): Find out what should we show in this case.
}

void TraySupervisedUser::UpdateAfterLoginStatusChange(LoginStatus status) {
  SystemTrayDelegate* delegate = WmShell::Get()->system_tray_delegate();

  bool is_user_supervised = delegate->IsUserSupervised();
  if (status == status_ && is_user_supervised == is_user_supervised_)
    return;

  if (is_user_supervised && !delegate->IsUserChild() &&
      status_ != LoginStatus::LOCKED &&
      !delegate->GetSupervisedUserManager().empty())
    CreateOrUpdateSupervisedWarningNotification();

  status_ = status;
  is_user_supervised_ = is_user_supervised;
}

void TraySupervisedUser::CreateOrUpdateNotification(
    const base::string16& new_message) {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  std::unique_ptr<Notification> notification(
      message_center::Notification::CreateSystemNotification(
          kNotificationId, base::string16() /* no title */, new_message,
          bundle.GetImageNamed(GetSupervisedUserIconId()),
          system_notifier::kNotifierSupervisedUser,
          base::Closure() /* null callback */));
  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

void TraySupervisedUser::CreateOrUpdateSupervisedWarningNotification() {
  SystemTrayDelegate* delegate = WmShell::Get()->system_tray_delegate();
  CreateOrUpdateNotification(delegate->GetSupervisedUserMessage());
}

void TraySupervisedUser::OnCustodianInfoChanged() {
  SystemTrayDelegate* delegate = WmShell::Get()->system_tray_delegate();
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
  SystemTrayDelegate* delegate = WmShell::Get()->system_tray_delegate();

  // Not intended to be used for non-supervised users.
  CHECK(delegate->IsUserSupervised());

  if (delegate->IsUserChild())
    return IDR_AURA_UBER_TRAY_CHILD_USER;
  return IDR_AURA_UBER_TRAY_SUPERVISED_USER;
}

}  // namespace ash
