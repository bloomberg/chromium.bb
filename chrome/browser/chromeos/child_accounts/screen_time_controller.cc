// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/screen_time_controller.h"

#include <algorithm>
#include <string>
#include <utility>

#include "ash/public/interfaces/login_screen.mojom.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/optional.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/child_accounts/consumer_status_reporting_service.h"
#include "chrome/browser/chromeos/child_accounts/consumer_status_reporting_service_factory.h"
#include "chrome/browser/chromeos/child_accounts/parent_access_code/policy_config_source.h"
#include "chrome/browser/chromeos/child_accounts/time_limit_override.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/login_screen_client.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/browser_context.h"

namespace chromeos {

namespace {

constexpr base::TimeDelta kUsageTimeLimitWarningTime =
    base::TimeDelta::FromMinutes(15);

// Dictionary keys for prefs::kScreenTimeLastState.
constexpr char kScreenStateLocked[] = "locked";
constexpr char kScreenStateCurrentPolicyType[] = "active_policy";
constexpr char kScreenStateTimeUsageLimitEnabled[] = "time_usage_limit_enabled";
constexpr char kScreenStateRemainingUsage[] = "remaining_usage";
constexpr char kScreenStateUsageLimitStarted[] = "usage_limit_started";
constexpr char kScreenStateNextStateChangeTime[] = "next_state_change_time";
constexpr char kScreenStateNextPolicyType[] = "next_active_policy";
constexpr char kScreenStateNextUnlockTime[] = "next_unlock_time";

}  // namespace

// static
void ScreenTimeController::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kScreenTimeLastState);
  registry->RegisterDictionaryPref(prefs::kUsageTimeLimit);
  registry->RegisterDictionaryPref(prefs::kTimeLimitLocalOverride);
}

ScreenTimeController::ScreenTimeController(content::BrowserContext* context)
    : context_(context),
      pref_service_(Profile::FromBrowserContext(context)->GetPrefs()),
      clock_(base::DefaultClock::GetInstance()),
      next_state_timer_(std::make_unique<base::OneShotTimer>()),
      usage_time_limit_warning_timer_(std::make_unique<base::OneShotTimer>()),
      time_limit_notifier_(context) {
  session_manager::SessionManager::Get()->AddObserver(this);
  if (base::FeatureList::IsEnabled(features::kUsageTimeStateNotifier))
    UsageTimeStateNotifier::GetInstance()->AddObserver(this);

  system::TimezoneSettings::GetInstance()->AddObserver(this);
  chromeos::SystemClockClient::Get()->AddObserver(this);
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kUsageTimeLimit,
      base::BindRepeating(&ScreenTimeController::OnPolicyChanged,
                          base::Unretained(this)));

  if (base::FeatureList::IsEnabled(features::kParentAccessCode)) {
    auto config_source =
        std::make_unique<parent_access::PolicyConfigSource>(pref_service_);
    parent_access_service_ =
        std::make_unique<parent_access::ParentAccessService>(
            std::move(config_source));
    parent_access_service_->SetDelegate(this);
  }
}

ScreenTimeController::~ScreenTimeController() {
  if (base::FeatureList::IsEnabled(features::kParentAccessCode)) {
    parent_access_service_->SetDelegate(nullptr);
  }

  session_manager::SessionManager::Get()->RemoveObserver(this);
  if (base::FeatureList::IsEnabled(features::kUsageTimeStateNotifier))
    UsageTimeStateNotifier::GetInstance()->RemoveObserver(this);

  system::TimezoneSettings::GetInstance()->RemoveObserver(this);
  SystemClockClient::Get()->RemoveObserver(this);
}

void ScreenTimeController::AddObserver(Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void ScreenTimeController::RemoveObserver(Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

base::TimeDelta ScreenTimeController::GetScreenTimeDuration() {
  return ConsumerStatusReportingServiceFactory::GetForBrowserContext(context_)
      ->GetChildScreenTime();
}

void ScreenTimeController::OnAccessCodeValidation(bool result) {
  if (!result)
    return;

  if (!session_manager::SessionManager::Get()->IsScreenLocked())
    return;

  usage_time_limit::TimeLimitOverride local_override(
      usage_time_limit::TimeLimitOverride::Action::kUnlock, clock_->Now(),
      base::nullopt);
  // Replace previous local override stored in pref, because PAC can only be
  // entered if previous override is not active anymore.
  pref_service_->Set(prefs::kTimeLimitLocalOverride,
                     local_override.ToDictionary());
  pref_service_->CommitPendingWrite();

  CheckTimeLimit("OnAccessCodeValidation");
}

void ScreenTimeController::SetClocksForTesting(
    const base::Clock* clock,
    const base::TickClock* tick_clock,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  clock_ = clock;
  next_state_timer_ = std::make_unique<base::OneShotTimer>(tick_clock);
  next_state_timer_->SetTaskRunner(task_runner);
  usage_time_limit_warning_timer_ =
      std::make_unique<base::OneShotTimer>(tick_clock);
  usage_time_limit_warning_timer_->SetTaskRunner(task_runner);
}

void ScreenTimeController::NotifyUsageTimeLimitWarningForTesting() {
  for (Observer& observer : observers_)
    observer.UsageTimeLimitWarning();
}

void ScreenTimeController::CheckTimeLimit(const std::string& source) {
  VLOG(1) << "Checking time limits (source=" << source << ")";

  // Stop all timers. They will be rescheduled below.
  ResetStateTimers();
  ResetInSessionTimers();
  ResetWarningTimers();

  base::Time now = clock_->Now();
  const icu::TimeZone& time_zone =
      system::TimezoneSettings::GetInstance()->GetTimezone();
  base::Optional<usage_time_limit::State> last_state = GetLastStateFromPref();
  const base::DictionaryValue* time_limit =
      pref_service_->GetDictionary(prefs::kUsageTimeLimit);
  const base::DictionaryValue* local_override =
      pref_service_->GetDictionary(prefs::kTimeLimitLocalOverride);

  // TODO(agawronska): Usage timestamp should be passed instead of second |now|.
  usage_time_limit::State state = usage_time_limit::GetState(
      time_limit->CreateDeepCopy(), local_override, GetScreenTimeDuration(),
      now, now, &time_zone, last_state);
  SaveCurrentStateToPref(state);

  // Show/hide time limits message based on the policy enforcement.
  UpdateLockScreenState(
      state.is_locked, state.is_locked ? state.next_unlock_time : base::Time());
  VLOG(1) << "Screen should be locked is set to " << state.is_locked;

  if (state.is_locked) {
    DCHECK(!state.next_unlock_time.is_null());
    if (!session_manager::SessionManager::Get()->IsScreenLocked()) {
      // This status report are going to be done in EventBasedStatusReporting if
      // this feature is enabled.
      if (!base::FeatureList::IsEnabled(features::kEventBasedStatusReporting)) {
        VLOG(1) << "Request status report before locking screen.";
        ConsumerStatusReportingServiceFactory::GetForBrowserContext(context_)
            ->RequestImmediateStatusReport();
      }
      ForceScreenLockByPolicy(state.next_unlock_time);
    }
  } else {
    base::Optional<TimeLimitNotifier::LimitType> notification_type;
    switch (state.next_state_active_policy) {
      case usage_time_limit::ActivePolicies::kFixedLimit:
        notification_type = TimeLimitNotifier::LimitType::kBedTime;
        break;
      case usage_time_limit::ActivePolicies::kUsageLimit:
      case usage_time_limit::ActivePolicies::kOverride:
        notification_type = TimeLimitNotifier::LimitType::kScreenTime;
        break;
      case usage_time_limit::ActivePolicies::kNoActivePolicy:
        break;
      default:
        NOTREACHED();
    }

    if (notification_type.has_value()) {
      // Schedule notification based on the remaining screen time until lock.
      // TODO(crbug.com/898000): Dismiss a shown notification when it no longer
      // applies.
      const base::TimeDelta remaining_time = state.next_state_change_time - now;
      time_limit_notifier_.MaybeScheduleNotifications(notification_type.value(),
                                                      remaining_time);
    }

    if (base::FeatureList::IsEnabled(features::kUsageTimeStateNotifier))
      ScheduleUsageTimeLimitWarning(state);
  }

  // TODO(agawronska): We are creating UsageTimeLimitProcessor second time in
  // this method. Could expected reset time be returned as a part of the state?
  base::Time next_get_state_time = std::min(
      state.next_state_change_time,
      usage_time_limit::GetExpectedResetTime(time_limit->CreateDeepCopy(),
                                             local_override, now, &time_zone));
  if (!next_get_state_time.is_null()) {
    VLOG(1) << "Scheduling state change timer in " << next_get_state_time - now;
    next_state_timer_->Start(
        FROM_HERE, next_get_state_time - now,
        base::BindRepeating(&ScreenTimeController::CheckTimeLimit,
                            base::Unretained(this), "next_state_timer_"));
  }
}

void ScreenTimeController::ForceScreenLockByPolicy(
    base::Time next_unlock_time) {
  DCHECK(!session_manager::SessionManager::Get()->IsScreenLocked());

  // Avoid abrupt session restart that looks like a crash and happens when lock
  // screen is requested before sign in completion. It is safe, because time
  // limits will be reevaluated when session state changes to active.
  // TODO(agawronska): Remove the flag when it is confirmed that this does not
  // cause a bug (https://crbug.com/924844).
  if (base::FeatureList::IsEnabled(features::kDMServerOAuthForChildUser) &&
      session_manager::SessionManager::Get()->session_state() !=
          session_manager::SessionState::ACTIVE)
    return;

  chromeos::DBusThreadManager::Get()
      ->GetSessionManagerClient()
      ->RequestLockScreen();

  // Update the time limits message when the lock screen UI is ready.
  next_unlock_time_ = next_unlock_time;
}

void ScreenTimeController::UpdateLockScreenState(bool blocked,
                                                 base::Time next_unlock_time) {
  DCHECK(blocked || next_unlock_time.is_null());
  if (!session_manager::SessionManager::Get()->IsScreenLocked())
    return;

  AccountId account_id =
      chromeos::ProfileHelper::Get()
          ->GetUserByProfile(Profile::FromBrowserContext(context_))
          ->GetAccountId();
  ScreenLocker::default_screen_locker()->SetAuthEnabledForUser(
      account_id, !blocked,
      blocked ? next_unlock_time : base::Optional<base::Time>());
  if (base::FeatureList::IsEnabled(features::kParentAccessCode)) {
    LoginScreenClient::Get()->login_screen()->SetShowParentAccessButton(
        blocked);
  }
}

void ScreenTimeController::OnPolicyChanged() {
  CheckTimeLimit("OnPolicyChanged");
}

void ScreenTimeController::ResetStateTimers() {
  VLOG(1) << "Stopping state timers";
  next_state_timer_->Stop();
}

void ScreenTimeController::ResetWarningTimers() {
  VLOG(1) << "Stopping warning timers";
  usage_time_limit_warning_timer_->Stop();
}

void ScreenTimeController::ResetInSessionTimers() {
  VLOG(1) << "Stopping in-session timers";
  time_limit_notifier_.UnscheduleNotifications();
}

void ScreenTimeController::ScheduleUsageTimeLimitWarning(
    const usage_time_limit::State& state) {
  if (state.next_state_active_policy ==
          usage_time_limit::ActivePolicies::kUsageLimit &&
      UsageTimeStateNotifier::GetInstance()->GetState() ==
          UsageTimeStateNotifier::UsageTimeState::ACTIVE) {
    base::Time now = clock_->Now();
    base::TimeDelta time_until_next_state = state.next_state_change_time - now;
    base::TimeDelta delay =
        time_until_next_state < kUsageTimeLimitWarningTime
            ? base::TimeDelta()
            : time_until_next_state - kUsageTimeLimitWarningTime;

    VLOG(1) << "Scheduling usage time limit warning in " << delay.InMinutes()
            << " minutes.";
    usage_time_limit_warning_timer_->Start(
        FROM_HERE, delay,
        base::BindRepeating(&ScreenTimeController::UsageTimeLimitWarning,
                            base::Unretained(this)));
  }
}

void ScreenTimeController::SaveCurrentStateToPref(
    const usage_time_limit::State& state) {
  auto state_dict =
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY);

  state_dict->SetKey(kScreenStateLocked, base::Value(state.is_locked));
  state_dict->SetKey(kScreenStateCurrentPolicyType,
                     base::Value(static_cast<int>(state.active_policy)));
  state_dict->SetKey(kScreenStateTimeUsageLimitEnabled,
                     base::Value(state.is_time_usage_limit_enabled));
  state_dict->SetKey(kScreenStateRemainingUsage,
                     base::Value(base::checked_cast<int>(
                         state.remaining_usage.InMilliseconds())));
  state_dict->SetKey(kScreenStateUsageLimitStarted,
                     base::Value(state.time_usage_limit_started.ToDoubleT()));
  state_dict->SetKey(kScreenStateNextStateChangeTime,
                     base::Value(state.next_state_change_time.ToDoubleT()));
  state_dict->SetKey(
      kScreenStateNextPolicyType,
      base::Value(static_cast<int>(state.next_state_active_policy)));
  state_dict->SetKey(kScreenStateNextUnlockTime,
                     base::Value(state.next_unlock_time.ToDoubleT()));

  pref_service_->Set(prefs::kScreenTimeLastState, *state_dict);
  pref_service_->CommitPendingWrite();
}

base::Optional<usage_time_limit::State>
ScreenTimeController::GetLastStateFromPref() {
  const base::DictionaryValue* last_state =
      pref_service_->GetDictionary(prefs::kScreenTimeLastState);
  usage_time_limit::State result;
  if (last_state->empty())
    return base::nullopt;

  // Verify is_locked from the pref is a boolean value.
  const base::Value* is_locked = last_state->FindKey(kScreenStateLocked);
  if (!is_locked || !is_locked->is_bool())
    return base::nullopt;
  result.is_locked = is_locked->GetBool();

  // Verify active policy type is a value of usage_time_limit::ActivePolicies.
  const base::Value* active_policy =
      last_state->FindKey(kScreenStateCurrentPolicyType);
  // TODO(crbug.com/823536): Add kCount in usage_time_limit::ActivePolicies
  // instead of checking kUsageLimit here.
  if (!active_policy || !active_policy->is_int() ||
      active_policy->GetInt() < 0 ||
      active_policy->GetInt() >
          static_cast<int>(usage_time_limit::ActivePolicies::kUsageLimit)) {
    return base::nullopt;
  }
  result.active_policy =
      static_cast<usage_time_limit::ActivePolicies>(active_policy->GetInt());

  // Verify time_usage_limit_enabled from the pref is a boolean value.
  const base::Value* time_usage_limit_enabled =
      last_state->FindKey(kScreenStateTimeUsageLimitEnabled);
  if (!time_usage_limit_enabled || !time_usage_limit_enabled->is_bool())
    return base::nullopt;
  result.is_time_usage_limit_enabled = time_usage_limit_enabled->GetBool();

  // Verify remaining_usage from the pref is a int value.
  const base::Value* remaining_usage =
      last_state->FindKey(kScreenStateRemainingUsage);
  if (!remaining_usage || !remaining_usage->is_int())
    return base::nullopt;
  result.remaining_usage =
      base::TimeDelta::FromMilliseconds(remaining_usage->GetInt());

  // Verify time_usage_limit_started from the pref is a double value.
  const base::Value* time_usage_limit_started =
      last_state->FindKey(kScreenStateUsageLimitStarted);
  if (!time_usage_limit_started || !time_usage_limit_started->is_double())
    return base::nullopt;
  result.time_usage_limit_started =
      base::Time::FromDoubleT(time_usage_limit_started->GetDouble());

  // Verify next_state_change_time from the pref is a double value.
  const base::Value* next_state_change_time =
      last_state->FindKey(kScreenStateNextStateChangeTime);
  if (!next_state_change_time || !next_state_change_time->is_double())
    return base::nullopt;
  result.next_state_change_time =
      base::Time::FromDoubleT(next_state_change_time->GetDouble());

  // Verify next policy type is a value of usage_time_limit::ActivePolicies.
  const base::Value* next_active_policy =
      last_state->FindKey(kScreenStateNextPolicyType);
  if (!next_active_policy || !next_active_policy->is_int() ||
      next_active_policy->GetInt() < 0 ||
      next_active_policy->GetInt() >
          static_cast<int>(usage_time_limit::ActivePolicies::kUsageLimit)) {
    return base::nullopt;
  }
  result.next_state_active_policy =
      static_cast<usage_time_limit::ActivePolicies>(
          next_active_policy->GetInt());

  // Verify next_unlock_time from the pref is a double value.
  const base::Value* next_unlock_time =
      last_state->FindKey(kScreenStateNextUnlockTime);
  if (!next_unlock_time || !next_unlock_time->is_double())
    return base::nullopt;
  result.next_unlock_time =
      base::Time::FromDoubleT(next_unlock_time->GetDouble());
  return result;
}

void ScreenTimeController::UsageTimeLimitWarning() {
  base::Time now = clock_->Now();
  const icu::TimeZone& time_zone =
      system::TimezoneSettings::GetInstance()->GetTimezone();
  const base::DictionaryValue* time_limit =
      pref_service_->GetDictionary(prefs::kUsageTimeLimit);
  const base::DictionaryValue* local_override =
      pref_service_->GetDictionary(prefs::kTimeLimitLocalOverride);

  base::Optional<base::TimeDelta> remaining_usage =
      usage_time_limit::GetRemainingTimeUsage(
          time_limit->CreateDeepCopy(), local_override, now,
          GetScreenTimeDuration(), &time_zone);

  // Remaining time usage can be bigger than |kUsageTimeLimitWarningTime|
  // because it is counted in another class so the timers might be called with
  // some delay. We must ensure that the observers will be called when the
  // usage time is less than |kUsageTimeLimitWarningTime|.
  if (remaining_usage && remaining_usage < kUsageTimeLimitWarningTime) {
    for (Observer& observer : observers_)
      observer.UsageTimeLimitWarning();
  } else {
    CheckTimeLimit("UsageTimeLimitWarning");
  }
}

void ScreenTimeController::OnSessionStateChanged() {
  session_manager::SessionState session_state =
      session_manager::SessionManager::Get()->session_state();
  if (base::FeatureList::IsEnabled(features::kUsageTimeStateNotifier)) {
    if (session_state == session_manager::SessionState::LOCKED &&
        next_unlock_time_) {
      UpdateLockScreenState(true /*blocked*/, next_unlock_time_.value());
      next_unlock_time_.reset();
    }
    return;
  }

  if (session_state == session_manager::SessionState::LOCKED) {
    if (next_unlock_time_) {
      UpdateLockScreenState(true /*blocked*/, next_unlock_time_.value());
      next_unlock_time_.reset();
    }
    ResetInSessionTimers();
  } else if (session_state == session_manager::SessionState::ACTIVE) {
    CheckTimeLimit("OnSessionStateChanged");
  }
}

void ScreenTimeController::OnUsageTimeStateChange(
    const UsageTimeStateNotifier::UsageTimeState state) {
  if (state == UsageTimeStateNotifier::UsageTimeState::INACTIVE) {
    ResetWarningTimers();
    ResetInSessionTimers();
  } else {
    CheckTimeLimit("OnUsageTimeStateChange");
  }
}

void ScreenTimeController::TimezoneChanged(const icu::TimeZone& timezone) {
  CheckTimeLimit("TimezoneChanged");
}

void ScreenTimeController::SystemClockUpdated() {
  CheckTimeLimit("SystemClockUpdated");
}

}  // namespace chromeos
