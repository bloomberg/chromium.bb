// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/system_tray_item.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray.h"
#include "ui/views/view.h"

namespace ash {

SystemTrayItem::SystemTrayItem() {
}

SystemTrayItem::~SystemTrayItem() {
}

views::View* SystemTrayItem::CreateTrayView(user::LoginStatus status) {
  return NULL;
}

views::View* SystemTrayItem::CreateDefaultView(user::LoginStatus status) {
  return NULL;
}

views::View* SystemTrayItem::CreateDetailedView(user::LoginStatus status) {
  return NULL;
}

views::View* SystemTrayItem::CreateNotificationView(user::LoginStatus status) {
  return NULL;
}

void SystemTrayItem::DestroyTrayView() {
}

void SystemTrayItem::DestroyDefaultView() {
}

void SystemTrayItem::DestroyDetailedView() {
}

void SystemTrayItem::DestroyNotificationView() {
}

void SystemTrayItem::TransitionDetailedView() {
  Shell::GetInstance()->tray()->ShowDetailedView(this, 0, true,
      BUBBLE_USE_EXISTING);
}

void SystemTrayItem::UpdateAfterLoginStatusChange(user::LoginStatus status) {
}

void SystemTrayItem::PopupDetailedView(int for_seconds, bool activate) {
  Shell::GetInstance()->tray()->ShowDetailedView(this, for_seconds, activate,
      BUBBLE_CREATE_NEW);
}

void SystemTrayItem::SetDetailedViewCloseDelay(int for_seconds) {
  Shell::GetInstance()->tray()->SetDetailedViewCloseDelay(for_seconds);
}

void SystemTrayItem::HideDetailedView() {
  Shell::GetInstance()->tray()->HideDetailedView(this);
}

void SystemTrayItem::ShowNotificationView() {
  Shell::GetInstance()->tray()->ShowNotificationView(this);
}

void SystemTrayItem::HideNotificationView() {
  Shell::GetInstance()->tray()->HideNotificationView(this);
}

}  // namespace ash
