// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/date/tray_date.h"

#include "ash/shell.h"
#include "ash/system/date/date_default_view.h"
#include "ash/system/date/date_view.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_item_view.h"

#if defined(OS_CHROMEOS)
#include "ash/system/chromeos/system_clock_observer.h"
#endif

namespace ash {

TrayDate::TrayDate(SystemTray* system_tray)
    : SystemTrayItem(system_tray),
      time_tray_(NULL),
      default_view_(NULL),
      login_status_(user::LOGGED_IN_NONE) {
#if defined(OS_CHROMEOS)
  system_clock_observer_.reset(new SystemClockObserver());
#endif
  Shell::GetInstance()->system_tray_notifier()->AddClockObserver(this);
}

TrayDate::~TrayDate() {
  Shell::GetInstance()->system_tray_notifier()->RemoveClockObserver(this);
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

views::View* TrayDate::CreateDefaultViewForTesting(user::LoginStatus status) {
  return CreateDefaultView(status);
}

views::View* TrayDate::CreateTrayView(user::LoginStatus status) {
  CHECK(time_tray_ == NULL);
  ClockLayout clock_layout =
      (system_tray()->shelf_alignment() == SHELF_ALIGNMENT_BOTTOM ||
       system_tray()->shelf_alignment() == SHELF_ALIGNMENT_TOP) ?
          HORIZONTAL_CLOCK : VERTICAL_CLOCK;
  time_tray_ = new tray::TimeView(clock_layout);
  views::View* view = new TrayItemView(this);
  view->AddChildView(time_tray_);
  return view;
}

views::View* TrayDate::CreateDefaultView(user::LoginStatus status) {
  default_view_ = new DateDefaultView(status);

#if defined(OS_CHROMEOS)
  // Save the login status we created the view with.
  login_status_ = status;

  OnSystemClockCanSetTimeChanged(system_clock_observer_->can_set_time());
#endif
  return default_view_;
}

views::View* TrayDate::CreateDetailedView(user::LoginStatus status) {
  return NULL;
}

void TrayDate::DestroyTrayView() {
  time_tray_ = NULL;
}

void TrayDate::DestroyDefaultView() {
  default_view_ = NULL;
}

void TrayDate::DestroyDetailedView() {
}

void TrayDate::UpdateAfterLoginStatusChange(user::LoginStatus status) {
}

void TrayDate::UpdateAfterShelfAlignmentChange(ShelfAlignment alignment) {
  if (time_tray_) {
    ClockLayout clock_layout = (alignment == SHELF_ALIGNMENT_BOTTOM ||
        alignment == SHELF_ALIGNMENT_TOP) ?
            HORIZONTAL_CLOCK : VERTICAL_CLOCK;
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
  if (default_view_ && login_status_ == user::LOGGED_IN_NONE) {
    default_view_->GetDateView()->SetAction(
        can_set_time ? TrayDate::SET_SYSTEM_TIME : TrayDate::NONE);
  }
}

void TrayDate::Refresh() {
  if (time_tray_)
    time_tray_->UpdateText();
}

}  // namespace ash
