// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/login_metrics_recorder.h"

#include "ash/login/ui/lock_screen.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"

namespace ash {

LoginMetricsRecorder::LoginMetricsRecorder() = default;
LoginMetricsRecorder::~LoginMetricsRecorder() = default;

void LoginMetricsRecorder::SetAuthMethod(AuthMethod method) {
  DCHECK_NE(method, AuthMethod::kMethodCount);
  if (!ash::LockScreen::IsShown() && !enabled_for_testing_)
    return;

  // Record usage of PIN / Password / Smartlock in lock screen.
  const bool is_tablet_mode = Shell::Get()
                                  ->tablet_mode_controller()
                                  ->IsTabletModeWindowManagerEnabled();
  if (is_tablet_mode) {
    UMA_HISTOGRAM_ENUMERATION("Ash.Login.Lock.AuthMethod.Used.TabletMode",
                              method, AuthMethod::kMethodCount);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Ash.Login.Lock.AuthMethod.Used.ClamShellMode",
                              method, AuthMethod::kMethodCount);
  }

  if (last_auth_method_ != method) {
    // Record switching between unlock methods.
    UMA_HISTOGRAM_ENUMERATION("Ash.Login.Lock.AuthMethod.Switched",
                              FindSwitchType(last_auth_method_, method),
                              AuthMethodSwitchType::kSwitchTypeCount);

    last_auth_method_ = method;
  }
}

void LoginMetricsRecorder::Reset() {
  // Reset local state.
  last_auth_method_ = AuthMethod::kPassword;
}

void LoginMetricsRecorder::RecordNumLoginAttempts(int num_attempt,
                                                  bool success) {
  if (!ash::LockScreen::IsShown() && !enabled_for_testing_)
    return;

  if (success) {
    UMA_HISTOGRAM_COUNTS_100("Ash.Login.Lock.NumPasswordAttempts.UntilSuccess",
                             num_attempt);
  } else {
    UMA_HISTOGRAM_COUNTS_100("Ash.Login.Lock.NumPasswordAttempts.UntilFailure",
                             num_attempt);
  }
}

void LoginMetricsRecorder::RecordUserClickEventOnLockScreen(
    LockScreenUserClickTarget target) {
  // Cancel button from LoginShelfView is converted to kTargetCount because it
  // is not part of the lock screen.
  if (target == LockScreenUserClickTarget::kTargetCount)
    return;

  if (!ash::LockScreen::IsShown() && !enabled_for_testing_)
    return;

  UMA_HISTOGRAM_ENUMERATION("Ash.Login.Lock.UserClicks", target,
                            LockScreenUserClickTarget::kTargetCount);
}

void LoginMetricsRecorder::EnableForTesting() {
  enabled_for_testing_ = true;
}

// static
LoginMetricsRecorder::AuthMethodSwitchType LoginMetricsRecorder::FindSwitchType(
    AuthMethod previous,
    AuthMethod current) {
  DCHECK_NE(previous, current);
  switch (previous) {
    case AuthMethod::kPassword:
      return current == AuthMethod::kPin
                 ? AuthMethodSwitchType::kPasswordToPin
                 : AuthMethodSwitchType::kPasswordToSmartlock;
    case AuthMethod::kPin:
      return current == AuthMethod::kPassword
                 ? AuthMethodSwitchType::kPinToPassword
                 : AuthMethodSwitchType::kPinToSmartlock;
    case AuthMethod::kSmartlock:
      return current == AuthMethod::kPassword
                 ? AuthMethodSwitchType::kSmartlockToPassword
                 : AuthMethodSwitchType::kSmartlockToPin;
    case AuthMethod::kMethodCount:
      NOTREACHED();
      return AuthMethodSwitchType::kSwitchTypeCount;
  }
}

}  // namespace ash
