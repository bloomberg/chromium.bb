// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/idle_action_warning_observer.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/power/idle_action_warning_dialog_view.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace {

// DO NOT REORDER - used to report metrics.
enum class IdleLogoutWarningEvent {
  kShown = 0,
  kCanceled = 1,
  kMaxValue = kCanceled
};

void ReportMetricsForDemoMode(IdleLogoutWarningEvent event) {
  if (chromeos::DemoSession::IsDeviceInDemoMode())
    UMA_HISTOGRAM_ENUMERATION("DemoMode.IdleLogoutWarningEvent", event);
}

}  // namespace

namespace chromeos {

IdleActionWarningObserver::IdleActionWarningObserver() {
  PowerManagerClient::Get()->AddObserver(this);
}

IdleActionWarningObserver::~IdleActionWarningObserver() {
  PowerManagerClient::Get()->RemoveObserver(this);
  if (warning_dialog_) {
    warning_dialog_->GetWidget()->RemoveObserver(this);
    warning_dialog_->CloseDialog();
  }
}

void IdleActionWarningObserver::IdleActionImminent(
    const base::TimeDelta& time_until_idle_action) {
  const base::TimeTicks idle_action_time =
      base::TimeTicks::Now() + time_until_idle_action;
  if (warning_dialog_) {
    warning_dialog_->Update(idle_action_time);
  } else {
    warning_dialog_ = new IdleActionWarningDialogView(idle_action_time);
    warning_dialog_->GetWidget()->AddObserver(this);
    ReportMetricsForDemoMode(IdleLogoutWarningEvent::kShown);
  }
}

void IdleActionWarningObserver::IdleActionDeferred() {
  if (warning_dialog_) {
    warning_dialog_->CloseDialog();
    ReportMetricsForDemoMode(IdleLogoutWarningEvent::kCanceled);
  }
}

void IdleActionWarningObserver::OnWidgetClosing(views::Widget* widget) {
  DCHECK(warning_dialog_);
  DCHECK_EQ(widget, warning_dialog_->GetWidget());
  warning_dialog_->GetWidget()->RemoveObserver(this);
  warning_dialog_ = nullptr;
}

}  // namespace chromeos
