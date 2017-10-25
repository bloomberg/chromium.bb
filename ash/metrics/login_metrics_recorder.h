// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_METRICS_LOGIN_METRICS_RECORDER_H_
#define ASH_METRICS_LOGIN_METRICS_RECORDER_H_

#include "ash/ash_export.h"
#include "base/macros.h"

namespace ash {

// A metrics recorder that records login authentication related metrics.
// This keeps track of the last authentication method we used and records
// switching between different authentication methods.
// This is tied to UserMetricsRecorder lifetime.
class ASH_EXPORT LoginMetricsRecorder {
 public:
  // Authentication method to unlock the screen. This enum is used to back an
  // UMA histogram and new values should be inserted immediately above
  // kMethodCount.
  enum class AuthMethod {
    kPassword = 0,
    kPin,
    kSmartlock,
    kMethodCount,
  };

  // The type of switching between auth methods. This enum is used to back an
  // UMA histogram and new values should be inserted immediately above
  // kSwitchTypeCount.
  enum class AuthMethodSwitchType {
    kPasswordToPin = 0,
    kPasswordToSmartlock,
    kPinToPassword,
    kPinToSmartlock,
    kSmartlockToPin,
    kSmartlockToPassword,
    kSwitchTypeCount,
  };

  // User clicks target on the lock screen. This enum is used to back an UMA
  // histogram and new values should be inserted immediately above kTargetCount.
  enum class LockScreenUserClickTarget {
    kShutDownButton = 0,
    kRestartButton,
    kSignOutButton,
    kCloseNoteButton,
    kSystemTray,
    kVirtualKeyboardTray,
    kImeTray,
    kNotificationTray,
    kLockScreenNoteActionButton,
    kTargetCount,
  };

  LoginMetricsRecorder();
  ~LoginMetricsRecorder();

  // Called when user attempts authentication using AuthMethod |type|.
  void SetAuthMethod(AuthMethod type);

  // Called when lock state changed.
  void Reset();

  // Used to record UMA stats.
  void RecordNumLoginAttempts(int num_attempt, bool success);
  void RecordUserClickEventOnLockScreen(LockScreenUserClickTarget target);

  void EnableForTesting();
  bool enabled_for_testing() { return enabled_for_testing_; };

 private:
  static AuthMethodSwitchType FindSwitchType(AuthMethod previous,
                                             AuthMethod current);

  AuthMethod last_auth_method_ = AuthMethod::kPassword;
  bool enabled_for_testing_ = false;

  DISALLOW_COPY_AND_ASSIGN(LoginMetricsRecorder);
};

}  // namespace ash

#endif  // ASH_METRICS_LOGIN_METRICS_RECORDER_H_
