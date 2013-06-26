// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/managed/tray_locally_managed_user.h"

#include "ash/system/chromeos/label_tray_view.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_notification_view.h"
#include "ash/system/user/login_status.h"
#include "base/logging.h"
#include "grit/ash_resources.h"

namespace ash {
namespace internal {

namespace {

class ManagedUserNotificationView : public TrayNotificationView {
 public:
  explicit ManagedUserNotificationView(TrayLocallyManagedUser* tray_managed)
      : TrayNotificationView(tray_managed, IDR_AURA_UBER_TRAY_MANAGED_USER),
        tray_managed_(tray_managed) {
    CreateMessageView();
    InitView(message_view_);
  }

  void Update() {
    CreateMessageView();
    UpdateView(message_view_);
  }

 private:
  void CreateMessageView() {
    message_view_ = new LabelTrayView(tray_managed_, 0);
    base::string16 message = Shell::GetInstance()->system_tray_delegate()->
        GetLocallyManagedUserMessage();
    message_view_->SetMessage(message);
  }

  LabelTrayView* message_view_;
  TrayLocallyManagedUser* tray_managed_;

  DISALLOW_COPY_AND_ASSIGN(ManagedUserNotificationView);
};

} // namespace

TrayLocallyManagedUser::TrayLocallyManagedUser(SystemTray* system_tray)
    : SystemTrayItem(system_tray),
      tray_view_(NULL),
      notification_view_(NULL),
      status_(ash::user::LOGGED_IN_NONE) {
}

TrayLocallyManagedUser::~TrayLocallyManagedUser() {
}

void TrayLocallyManagedUser::UpdateMessage() {
  base::string16 message = Shell::GetInstance()->system_tray_delegate()->
      GetLocallyManagedUserMessage();
  if (tray_view_)
    tray_view_->SetMessage(message);
  if (notification_view_)
    tray_view_->SetMessage(message);
}

views::View* TrayLocallyManagedUser::CreateNotificationView(
    user::LoginStatus status) {
  CHECK(!notification_view_);
  if (status != ash::user::LOGGED_IN_LOCALLY_MANAGED)
    return NULL;
  notification_view_ = new ManagedUserNotificationView(this);
  return notification_view_;
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

void TrayLocallyManagedUser::DestroyNotificationView() {
  notification_view_ = NULL;
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
    ShowNotificationView();
  }
  status_ = status;
}

} // namespace internal
} // namespace ash

