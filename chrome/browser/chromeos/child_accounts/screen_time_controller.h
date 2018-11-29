// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_SCREEN_TIME_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_SCREEN_TIME_CONTROLLER_H_

#include <memory>

#include "base/time/time.h"
#include "chrome/browser/chromeos/child_accounts/time_limit_notifier.h"
#include "chrome/browser/chromeos/child_accounts/usage_time_limit_processor.h"
#include "chromeos/dbus/system_clock_client.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/session_manager/core/session_manager_observer.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class Clock;
class TickClock;
class OneShotTimer;
}  // namespace base

namespace content {
class BrowserContext;
}

namespace chromeos {

// The controller to track each user's screen time usage and inquiry time limit
// processor (a component to calculate state based on policy settings and time
// usage) when necessary to determine the current lock screen state.
// Schedule notifications and lock/unlock screen based on the processor output.
class ScreenTimeController : public KeyedService,
                             public session_manager::SessionManagerObserver,
                             public system::TimezoneSettings::Observer,
                             public chromeos::SystemClockClient::Observer {
 public:
  // Registers preferences.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  explicit ScreenTimeController(content::BrowserContext* context);
  ~ScreenTimeController() override;

  // Returns the child's screen time duration. This is how long the child has
  // used the device today (since the last reset).
  base::TimeDelta GetScreenTimeDuration();

  void SetClocksForTesting(const base::Clock* clock,
                           const base::TickClock* tick_clock);

 private:
  // Call time limit processor for new state.
  void CheckTimeLimit(const std::string& source);

  // Request to lock the screen and show the time limits message when the screen
  // is locked.
  void ForceScreenLockByPolicy(base::Time next_unlock_time);

  // Update visibility and content of the time limits message in the lock
  // screen.
  // |visible|: If true, user authentication is disabled and a message is shown
  //            to indicate when user will be able to unlock the screen.
  //            If false, message is dismissed and user is able to unlock
  //            immediately.
  // |next_unlock_time|: When user will be able to unlock the screen, only valid
  //                     when |visible| is true.
  void UpdateTimeLimitsMessage(bool visible, base::Time next_unlock_time);

  // Called when the policy of time limits changes.
  void OnPolicyChanged();

  // Reset any currently running timers.
  void ResetStateTimers();
  void ResetInSessionTimers();

  // Save the |state| to |prefs::kScreenTimeLastState|.
  void SaveCurrentStateToPref(const usage_time_limit::State& state);

  // Get the last calculated |state| from |prefs::kScreenTimeLastState|, if it
  // exists.
  base::Optional<usage_time_limit::State> GetLastStateFromPref();

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

  // system::TimezoneSettings::Observer:
  void TimezoneChanged(const icu::TimeZone& timezone) override;

  // chromeos::SystemClockClient::Observer:
  void SystemClockUpdated() override;

  content::BrowserContext* context_;
  PrefService* pref_service_;

  // Points to the base::DefaultClock by default.
  const base::Clock* clock_;

  // Timer scheduled for when the next lock screen state change event is
  // expected to happen, e.g. when bedtime is over or the usage limit ends.
  std::unique_ptr<base::OneShotTimer> next_state_timer_;

  // Used to set up timers when a time limit is approaching.
  TimeLimitNotifier time_limit_notifier_;

  PrefChangeRegistrar pref_change_registrar_;

  // Used to update the time limits message, if any, when screen is locked.
  base::Optional<base::Time> next_unlock_time_;

  DISALLOW_COPY_AND_ASSIGN(ScreenTimeController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_SCREEN_TIME_CONTROLLER_H_
