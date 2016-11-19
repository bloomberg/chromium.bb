// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/system_tray_item.h"

#include "ash/common/system/tray/system_tray.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/tray/tray_constants.h"
#include "base/timer/timer.h"
#include "ui/views/view.h"

namespace ash {

SystemTrayItem::SystemTrayItem(SystemTray* system_tray, UmaType uma_type)
    : system_tray_(system_tray), uma_type_(uma_type), restore_focus_(false) {}

SystemTrayItem::~SystemTrayItem() {}

views::View* SystemTrayItem::CreateTrayView(LoginStatus status) {
  return nullptr;
}

views::View* SystemTrayItem::CreateDefaultView(LoginStatus status) {
  return nullptr;
}

views::View* SystemTrayItem::CreateDetailedView(LoginStatus status) {
  return nullptr;
}

views::View* SystemTrayItem::CreateNotificationView(LoginStatus status) {
  return nullptr;
}

void SystemTrayItem::DestroyTrayView() {}

void SystemTrayItem::DestroyDefaultView() {}

void SystemTrayItem::DestroyDetailedView() {}

void SystemTrayItem::DestroyNotificationView() {}

void SystemTrayItem::TransitionDetailedView() {
  const int transition_delay =
      GetTrayConstant(TRAY_POPUP_TRANSITION_TO_DETAILED_DELAY);
  if (transition_delay <= 0) {
    DoTransitionToDetailedView();
    return;
  }
  transition_delay_timer_.reset(new base::OneShotTimer());
  transition_delay_timer_->Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(transition_delay), this,
      &SystemTrayItem::DoTransitionToDetailedView);
}

void SystemTrayItem::UpdateAfterLoginStatusChange(LoginStatus status) {}

void SystemTrayItem::UpdateAfterShelfAlignmentChange(ShelfAlignment alignment) {
}

void SystemTrayItem::PopupDetailedView(int for_seconds, bool activate) {
  system_tray()->ShowDetailedView(this, for_seconds, activate,
                                  BUBBLE_CREATE_NEW);
}

void SystemTrayItem::SetDetailedViewCloseDelay(int for_seconds) {
  system_tray()->SetDetailedViewCloseDelay(for_seconds);
}

void SystemTrayItem::HideDetailedView(bool animate) {
  system_tray()->HideDetailedView(this, animate);
}

void SystemTrayItem::ShowNotificationView() {
  system_tray()->ShowNotificationView(this);
}

void SystemTrayItem::HideNotificationView() {
  system_tray()->HideNotificationView(this);
}

bool SystemTrayItem::ShouldShowShelf() const {
  return true;
}

void SystemTrayItem::DoTransitionToDetailedView() {
  transition_delay_timer_.reset();
  system_tray()->ShowDetailedView(this, 0, true, BUBBLE_USE_EXISTING);
}

}  // namespace ash
