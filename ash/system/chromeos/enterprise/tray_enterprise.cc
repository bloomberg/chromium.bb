// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/enterprise/tray_enterprise.h"

#include "ash/shell.h"
#include "ash/system/chromeos/label_tray_view.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/user/login_status.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "grit/ash_resources.h"

namespace ash {

TrayEnterprise::TrayEnterprise(SystemTray* system_tray)
    : SystemTrayItem(system_tray),
      tray_view_(NULL) {
  Shell::GetInstance()->system_tray_notifier()->
      AddEnterpriseDomainObserver(this);
}

TrayEnterprise::~TrayEnterprise() {
  Shell::GetInstance()->system_tray_notifier()->
      RemoveEnterpriseDomainObserver(this);
}

void TrayEnterprise::UpdateEnterpriseMessage() {
  base::string16 message = Shell::GetInstance()->system_tray_delegate()->
      GetEnterpriseMessage();
  if (tray_view_)
    tray_view_->SetMessage(message);
}

views::View* TrayEnterprise::CreateDefaultView(user::LoginStatus status) {
  CHECK(tray_view_ == NULL);
  // For public accounts, enterprise ownership is indicated in the user details
  // instead.
  if (status == ash::user::LOGGED_IN_PUBLIC)
    return NULL;
  tray_view_ = new LabelTrayView(this, IDR_AURA_UBER_TRAY_ENTERPRISE);
  UpdateEnterpriseMessage();
  return tray_view_;
}

void TrayEnterprise::DestroyDefaultView() {
  tray_view_ = NULL;
}

void TrayEnterprise::OnEnterpriseDomainChanged() {
  UpdateEnterpriseMessage();
}

void TrayEnterprise::OnViewClicked(views::View* sender) {
  Shell::GetInstance()->system_tray_delegate()->ShowEnterpriseInfo();
}

} // namespace ash
