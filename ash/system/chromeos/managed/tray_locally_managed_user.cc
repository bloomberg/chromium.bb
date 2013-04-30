// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/managed/tray_locally_managed_user.h"

#include "ash/system/chromeos/label_tray_view.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/user/login_status.h"
#include "base/logging.h"
#include "grit/ash_resources.h"

namespace ash {
namespace internal {

TrayLocallyManagedUser::TrayLocallyManagedUser(SystemTray* system_tray)
    : SystemTrayItem(system_tray),
      tray_view_(NULL) {
}

TrayLocallyManagedUser::~TrayLocallyManagedUser() {
}

void TrayLocallyManagedUser::UpdateMessage() {
  base::string16 message = Shell::GetInstance()->system_tray_delegate()->
      GetLocallyManagedUserMessage();
  if (tray_view_)
    tray_view_->SetMessage(message);
}

views::View* TrayLocallyManagedUser::CreateDefaultView(
    user::LoginStatus status) {
  CHECK(tray_view_ == NULL);
  if (status != ash::user::LOGGED_IN_LOCALLY_MANAGED)
    return NULL;

  // TODO(antrim): replace to appropriate icon when there is one.
  tray_view_ = new LabelTrayView(this, IDR_AURA_UBER_TRAY_ENTERPRISE_DARK);
  UpdateMessage();
  return tray_view_;
}

void TrayLocallyManagedUser::DestroyDefaultView() {
  tray_view_ = NULL;
}

void TrayLocallyManagedUser::OnViewClicked(views::View* sender) {
  Shell::GetInstance()->system_tray_delegate()->ShowLocallyManagedUserInfo();
}

} // namespace internal
} // namespace ash

