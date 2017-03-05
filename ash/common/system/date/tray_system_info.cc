// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/date/tray_system_info.h"

#include "ash/common/shelf/wm_shelf_util.h"
#include "ash/common/system/chromeos/system_clock_observer.h"
#include "ash/common/system/date/date_view.h"
#include "ash/common/system/date/system_info_default_view.h"
#include "ash/common/system/tray/system_tray.h"
#include "ash/common/system/tray/system_tray_notifier.h"
#include "ash/common/system/tray/tray_item_view.h"
#include "ash/common/wm_shell.h"

namespace ash {

TraySystemInfo::TraySystemInfo(SystemTray* system_tray)
    : SystemTrayItem(system_tray, UMA_DATE),
      tray_view_(nullptr),
      default_view_(nullptr),
      login_status_(LoginStatus::NOT_LOGGED_IN),
      system_clock_observer_(new SystemClockObserver()) {
  WmShell::Get()->system_tray_notifier()->AddClockObserver(this);
}

TraySystemInfo::~TraySystemInfo() {
  WmShell::Get()->system_tray_notifier()->RemoveClockObserver(this);
}

const tray::TimeView* TraySystemInfo::GetTimeTrayForTesting() const {
  return tray_view_;
}

const SystemInfoDefaultView* TraySystemInfo::GetDefaultViewForTesting() const {
  return default_view_;
}

views::View* TraySystemInfo::CreateDefaultViewForTesting(LoginStatus status) {
  return CreateDefaultView(status);
}

views::View* TraySystemInfo::CreateTrayView(LoginStatus status) {
  CHECK(tray_view_ == nullptr);
  tray::TimeView::ClockLayout clock_layout =
      IsHorizontalAlignment(system_tray()->shelf_alignment())
          ? tray::TimeView::ClockLayout::HORIZONTAL_CLOCK
          : tray::TimeView::ClockLayout::VERTICAL_CLOCK;
  tray_view_ = new tray::TimeView(clock_layout);
  views::View* view = new TrayItemView(this);
  view->AddChildView(tray_view_);
  return view;
}

views::View* TraySystemInfo::CreateDefaultView(LoginStatus status) {
  default_view_ = new SystemInfoDefaultView(this, status);

  // Save the login status we created the view with.
  login_status_ = status;

  OnSystemClockCanSetTimeChanged(system_clock_observer_->can_set_time());
  return default_view_;
}

void TraySystemInfo::DestroyTrayView() {
  tray_view_ = nullptr;
}

void TraySystemInfo::DestroyDefaultView() {
  default_view_ = nullptr;
}

void TraySystemInfo::UpdateAfterShelfAlignmentChange(ShelfAlignment alignment) {
  if (tray_view_) {
    tray::TimeView::ClockLayout clock_layout =
        IsHorizontalAlignment(alignment)
            ? tray::TimeView::ClockLayout::HORIZONTAL_CLOCK
            : tray::TimeView::ClockLayout::VERTICAL_CLOCK;
    tray_view_->UpdateClockLayout(clock_layout);
  }
}

void TraySystemInfo::OnDateFormatChanged() {
  UpdateTimeFormat();
}

void TraySystemInfo::OnSystemClockTimeUpdated() {
  UpdateTimeFormat();
}

void TraySystemInfo::OnSystemClockCanSetTimeChanged(bool can_set_time) {
  // Outside of a logged-in session, the date button should launch the set time
  // dialog if the time can be set.
  if (default_view_ && login_status_ == LoginStatus::NOT_LOGGED_IN) {
    default_view_->GetDateView()->SetAction(
        can_set_time ? tray::DateView::DateAction::SET_SYSTEM_TIME
                     : tray::DateView::DateAction::NONE);
  }
}

void TraySystemInfo::Refresh() {
  if (tray_view_)
    tray_view_->UpdateText();
}

void TraySystemInfo::UpdateTimeFormat() {
  if (tray_view_)
    tray_view_->UpdateTimeFormat();
  if (default_view_)
    default_view_->GetDateView()->UpdateTimeFormat();
}

}  // namespace ash
