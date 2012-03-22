// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/ime/tray_ime.h"

#include <vector>

#include "ash/shell.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/tray_views.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "ui/gfx/font.h"
#include "ui/views/controls/label.h"

namespace ash {
namespace internal {

TrayIME::TrayIME() {
}

TrayIME::~TrayIME() {
}

void TrayIME::UpdateTrayLabel() {
  IMEInfoList list;
  ash::Shell::GetInstance()->tray_delegate()->GetAvailableIMEList(&list);
  for (size_t i = 0; i < list.size(); i++) {
    if (list[i].selected) {
      tray_label_->SetText(list[i].short_name);
      break;
    }
  }
  tray_label_->SetVisible(list.size() > 1);
}

views::View* TrayIME::CreateTrayView(user::LoginStatus status) {
  tray_label_.reset(new views::Label);
  SetupLabelForTray(tray_label_.get());
  return tray_label_.get();
}

views::View* TrayIME::CreateDefaultView(user::LoginStatus status) {
  return NULL;
}

views::View* TrayIME::CreateDetailedView(user::LoginStatus status) {
  return NULL;
}

void TrayIME::DestroyTrayView() {
  tray_label_.reset();
}

void TrayIME::DestroyDefaultView() {
}

void TrayIME::DestroyDetailedView() {
}

void TrayIME::OnIMERefresh() {
  UpdateTrayLabel();
}

}  // namespace internal
}  // namespace ash
