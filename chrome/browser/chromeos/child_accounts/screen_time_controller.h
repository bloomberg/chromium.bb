// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_SCREEN_TIME_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_SCREEN_TIME_CONTROLLER_H_

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/session_manager/core/session_manager_observer.h"

class PrefRegistrySimple;
class PrefService;

namespace chromeos {

// The controller to track each user's screen time usage and inquiry time limit
// processor (a component to calculate state based on policy settings and time
// usage) when necessary to determine the current lock screen state.
// Schedule notifications and lock/unlock screen based on the processor output.
class ScreenTimeController : public KeyedService,
                             public session_manager::SessionManagerObserver {
 public:
  // Fake enum from time limit processor, should be removed when the processer
  // is ready.
  enum class ActivePolicy {
    kNoActivePolicy,
    kOverride,
    kFixedLimit,
    kUsageLimit
  };

  // Fake struct from time limit processor, should be removed when the
  // processer is ready.
  struct TimeLimitState {
    bool is_locked = false;
    ActivePolicy active_policy = ActivePolicy::kNoActivePolicy;
    bool is_time_usage_limit_enabled = false;
    base::TimeDelta remaining_usage = base::TimeDelta();
    base::Time next_state_change_time = base::Time();
    ActivePolicy next_state_active_policy = ActivePolicy::kNoActivePolicy;
  };

  // Registers preferences.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  explicit ScreenTimeController(PrefService* pref_service);
  ~ScreenTimeController() override;

  // Returns the screen time duration. This includes time in
  // kScreenTimeMinutesUsed plus time passed since |current_screen_start_time_|.
  base::TimeDelta GetScreenTimeDuration() const;

  // Call time limit processor for new state.
  void CheckTimeLimit();

 private:
  // Show and update the lock screen when necessary.
  // |force_lock_by_policy|: If true, force to lock the screen based on the
  //                         screen time policy.
  // |come_back_time|:       When the screen is available again.
  void LockScreen(bool force_lock_by_policy, base::Time come_back_time);

  // Show nofication when the screen is going to be locked.
  // |time_remaining|: time left before the lock down.
  void ShowNotification(base::TimeDelta time_remaining);

  // Reset time tracking relevant prefs and local timestamps.
  void RefreshScreenLimit();

  // Called when the policy of time limits changes.
  void OnPolicyChanged();

  // Reset any currently running timers.
  void ResetTimers();

  // Save the screen time progress when screen is locked, or user sign out or
  // power down the device.
  void SaveScreenTimeProgressBeforeExit();

  // Save the screen time progress periodically in case of a crash or power
  // outage.
  void SaveScreenTimeProgressPeriodically();

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

  PrefService* pref_service_;
  base::OneShotTimer warning_notification_timer_;
  base::OneShotTimer exit_notification_timer_;
  base::OneShotTimer next_state_timer_;
  base::RepeatingTimer save_screen_time_timer_;

  // Timestamp to keep track of the screen start time for the current active
  // screen. This timestamp is periodically updated by
  // SaveScreenTimeProgressPeriodically(), and is cleared when user exits the
  // active screen(lock, sign out, shutdown).
  base::Time current_screen_start_time_;

  // Timestamp to keep track of the screen start time when user starts using
  // the device for the first time of the day.
  // Used to calculate the screen time limit and this will be refreshed by
  // RefreshScreenLimit();
  base::Time first_screen_start_time_;

  DISALLOW_COPY_AND_ASSIGN(ScreenTimeController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_SCREEN_TIME_CONTROLLER_H_
