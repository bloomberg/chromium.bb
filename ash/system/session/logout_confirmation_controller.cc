// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/session/logout_confirmation_controller.h"

#include <utility>

#include "ash/login_status.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/system/session/logout_confirmation_dialog.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "base/location.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {
const int kLogoutConfirmationDelayInSeconds = 20;
}

LogoutConfirmationController::LogoutConfirmationController(
    const base::Closure& logout_closure)
    : clock_(new base::DefaultTickClock),
      logout_closure_(logout_closure),
      dialog_(NULL),
      logout_timer_(false, false),
      scoped_session_observer_(this) {
  if (Shell::HasInstance())
    Shell::Get()->system_tray_notifier()->AddLastWindowClosedObserver(this);
}

LogoutConfirmationController::~LogoutConfirmationController() {
  if (Shell::HasInstance())
    Shell::Get()->system_tray_notifier()->RemoveLastWindowClosedObserver(this);

  if (dialog_)
    dialog_->ControllerGone();
}

void LogoutConfirmationController::ConfirmLogout(base::TimeTicks logout_time) {
  if (!logout_time_.is_null() && logout_time >= logout_time_) {
    // If a confirmation dialog is already being shown and its countdown expires
    // no later than the |logout_time| requested now, keep the current dialog
    // open.
    return;
  }
  logout_time_ = logout_time;

  if (!dialog_) {
    // Show confirmation dialog unless this is a unit test without a Shell.
    if (Shell::HasInstance())
      dialog_ = new LogoutConfirmationDialog(this, logout_time_);
  } else {
    dialog_->Update(logout_time_);
  }

  logout_timer_.Start(FROM_HERE, logout_time_ - clock_->NowTicks(),
                      logout_closure_);
}

void LogoutConfirmationController::SetClockForTesting(
    std::unique_ptr<base::TickClock> clock) {
  clock_ = std::move(clock);
}

void LogoutConfirmationController::OnLockStateChanged(bool locked) {
  if (!locked || logout_time_.is_null())
    return;

  // If the screen is locked while a confirmation dialog is being shown, close
  // the dialog.
  logout_time_ = base::TimeTicks();
  if (dialog_)
    dialog_->GetWidget()->Close();
  logout_timer_.Stop();
}

void LogoutConfirmationController::OnLogoutConfirmed() {
  logout_timer_.Stop();
  logout_closure_.Run();
}

void LogoutConfirmationController::OnDialogClosed() {
  logout_time_ = base::TimeTicks();
  dialog_ = NULL;
  logout_timer_.Stop();
}

void LogoutConfirmationController::OnLastWindowClosed() {
  if (Shell::Get()->session_controller()->login_status() !=
      LoginStatus::PUBLIC) {
    return;
  }

  // Ask the user to confirm logout if a public session is in progress and the
  // screen is not locked.
  ConfirmLogout(
      base::TimeTicks::Now() +
      base::TimeDelta::FromSeconds(kLogoutConfirmationDelayInSeconds));
}

}  // namespace ash
