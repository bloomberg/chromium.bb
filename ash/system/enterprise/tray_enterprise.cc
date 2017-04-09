// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/enterprise/tray_enterprise.h"

#include "ash/login_status.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/system/tray/label_tray_view.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "base/logging.h"
#include "base/strings/string16.h"

namespace ash {

TrayEnterprise::TrayEnterprise(SystemTray* system_tray)
    : SystemTrayItem(system_tray, UMA_ENTERPRISE), tray_view_(nullptr) {
  Shell::Get()->system_tray_notifier()->AddEnterpriseDomainObserver(this);
}

TrayEnterprise::~TrayEnterprise() {
  Shell::Get()->system_tray_notifier()->RemoveEnterpriseDomainObserver(this);
}

void TrayEnterprise::UpdateEnterpriseMessage() {
  base::string16 message =
      Shell::Get()->system_tray_delegate()->GetEnterpriseMessage();
  if (tray_view_)
    tray_view_->SetMessage(message);
}

views::View* TrayEnterprise::CreateDefaultView(LoginStatus status) {
  DCHECK(!tray_view_);
  // For public accounts, enterprise ownership is indicated in the user details
  // instead.
  if (status == LoginStatus::PUBLIC)
    return nullptr;
  tray_view_ = new LabelTrayView(this, kSystemMenuBusinessIcon);
  UpdateEnterpriseMessage();
  return tray_view_;
}

void TrayEnterprise::DestroyDefaultView() {
  tray_view_ = nullptr;
}

void TrayEnterprise::OnEnterpriseDomainChanged() {
  UpdateEnterpriseMessage();
}

void TrayEnterprise::OnViewClicked(views::View* sender) {
  Shell::Get()->system_tray_delegate()->ShowEnterpriseInfo();
}

}  // namespace ash
