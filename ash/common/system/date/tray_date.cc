// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/date/tray_date.h"

#include "ash/common/shelf/wm_shelf_util.h"
#include "ash/common/system/date/date_default_view.h"
#include "ash/common/system/date/date_view.h"
#include "ash/common/system/tray/system_tray_notifier.h"
#include "ash/common/system/tray/tray_item_view.h"
#include "ash/common/wm_shell.h"
#include "ash/system/tray/system_tray.h"

#if defined(OS_CHROMEOS)
#include "ash/common/system/chromeos/system_clock_observer.h"
#endif

namespace ash {

TrayDate::TrayDate(SystemTray* system_tray)
    : SystemTrayItem(system_tray),
      time_tray_(NULL),
      default_view_(NULL),
      login_status_(LoginStatus::NOT_LOGGED_IN) {
#if defined(OS_CHROMEOS)
  system_clock_observer_.reset(new SystemClockObserver());
#endif
  WmShell::Get()->system_tray_notifier()->AddClockObserver(this);
}

TrayDate::~TrayDate() {
  WmShell::Get()->system_tray_notifier()->RemoveClockObserver(this);
}

views::View* TrayDate::GetHelpButtonView() const {
  if (!default_view_)
    return NULL;
  return default_view_->GetHelpButtonView();
}

const tray::TimeView* TrayDate::GetTimeTrayForTesting() const {
  return time_tray_;
}

const DateDefaultView* TrayDate::GetDefaultViewForTesting() const {
  return default_view_;
}

views::View* TrayDate::CreateDefaultViewForTesting(LoginStatus status) {
  return CreateDefaultView(status);
}

views::View* TrayDate::CreateTrayView(LoginStatus status) {
  CHECK(time_tray_ == NULL);
  ClockLayout clock_layout =
      system_tray()->shelf_alignment() == SHELF_ALIGNMENT_BOTTOM
          ? HORIZONTAL_CLOCK
          : VERTICAL_CLOCK;
  time_tray_ = new tray::TimeView(clock_layout);
  views::View* view = new TrayItemView(this);
  view->AddChildView(time_tray_);
  return view;
}

views::View* TrayDate::CreateDefaultView(LoginStatus status) {
  default_view_ = new DateDefaultView(status);

#if defined(OS_CHROMEOS)
  // Save the login status we created the view with.
  login_status_ = status;

  OnSystemClockCanSetTimeChanged(system_clock_observer_->can_set_time());
#endif
  return default_view_;
}

views::View* TrayDate::CreateDetailedView(LoginStatus status) {
  return NULL;
}

void TrayDate::DestroyTrayView() {
  time_tray_ = NULL;
}

void TrayDate::DestroyDefaultView() {
  default_view_ = NULL;
}

void TrayDate::DestroyDetailedView() {}

void TrayDate::UpdateAfterLoginStatusChange(LoginStatus status) {}

void TrayDate::UpdateAfterShelfAlignmentChange(ShelfAlignment alignment) {
  if (time_tray_) {
    ClockLayout clock_layout =
        IsHorizontalAlignment(alignment) ? HORIZONTAL_CLOCK : VERTICAL_CLOCK;
    time_tray_->UpdateClockLayout(clock_layout);
  }
}

void TrayDate::OnDateFormatChanged() {
  if (time_tray_)
    time_tray_->UpdateTimeFormat();
  if (default_view_)
    default_view_->GetDateView()->UpdateTimeFormat();
}

void TrayDate::OnSystemClockTimeUpdated() {
  if (time_tray_)
    time_tray_->UpdateTimeFormat();
  if (default_view_)
    default_view_->GetDateView()->UpdateTimeFormat();
}

void TrayDate::OnSystemClockCanSetTimeChanged(bool can_set_time) {
  // Outside of a logged-in session, the date button should launch the set time
  // dialog if the time can be set.
  if (default_view_ && login_status_ == LoginStatus::NOT_LOGGED_IN) {
    default_view_->GetDateView()->SetAction(
        can_set_time ? TrayDate::SET_SYSTEM_TIME : TrayDate::NONE);
  }
}

void TrayDate::Refresh() {
  if (time_tray_)
    time_tray_->UpdateText();
}

}  // namespace ash
