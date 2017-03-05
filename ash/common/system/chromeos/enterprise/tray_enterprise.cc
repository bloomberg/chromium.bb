// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/enterprise/tray_enterprise.h"

#include "ash/common/login_status.h"
#include "ash/common/system/tray/label_tray_view.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/tray/system_tray_notifier.h"
#include "ash/common/wm_shell.h"
#include "ash/resources/grit/ash_resources.h"
#include "base/logging.h"
#include "base/strings/string16.h"

namespace ash {

TrayEnterprise::TrayEnterprise(SystemTray* system_tray)
    : SystemTrayItem(system_tray, UMA_ENTERPRISE), tray_view_(nullptr) {
  WmShell::Get()->system_tray_notifier()->AddEnterpriseDomainObserver(this);
}

TrayEnterprise::~TrayEnterprise() {
  WmShell::Get()->system_tray_notifier()->RemoveEnterpriseDomainObserver(this);
}

void TrayEnterprise::UpdateEnterpriseMessage() {
  base::string16 message =
      WmShell::Get()->system_tray_delegate()->GetEnterpriseMessage();
  if (tray_view_)
    tray_view_->SetMessage(message);
}

views::View* TrayEnterprise::CreateDefaultView(LoginStatus status) {
  CHECK(tray_view_ == NULL);
  // For public accounts, enterprise ownership is indicated in the user details
  // instead.
  if (status == LoginStatus::PUBLIC)
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
  WmShell::Get()->system_tray_delegate()->ShowEnterpriseInfo();
}

}  // namespace ash
