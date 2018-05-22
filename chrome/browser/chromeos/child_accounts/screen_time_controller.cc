// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/screen_time_controller.h"

#include "ash/public/cpp/vector_icons/vector_icons.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/browser_context.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/message_center/public/cpp/notification.h"

namespace chromeos {

namespace {

constexpr base::TimeDelta kWarningNotificationTimeout =
    base::TimeDelta::FromMinutes(5);
constexpr base::TimeDelta kExitNotificationTimeout =
    base::TimeDelta::FromMinutes(1);
constexpr base::TimeDelta kScreenTimeUsageUpdateFrequency =
    base::TimeDelta::FromMinutes(1);

// The notification id. All the time limit notifications share the same id so
// that a subsequent notification can replace the previous one.
constexpr char kTimeLimitNotificationId[] = "time-limit-notification";

// The notifier id representing the app.
constexpr char kTimeLimitNotifierId[] = "family-link";

}  // namespace

// static
void ScreenTimeController::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterTimePref(prefs::kFirstScreenStartTime, base::Time());
  registry->RegisterTimePref(prefs::kCurrentScreenStartTime, base::Time());
  registry->RegisterIntegerPref(prefs::kScreenTimeMinutesUsed, 0);
}

ScreenTimeController::ScreenTimeController(content::BrowserContext* context)
    : context_(context),
      pref_service_(Profile::FromBrowserContext(context)->GetPrefs()) {
  // TODO(https://crbug.com/823536): observe the policy changes for: screen time
  // limit, bed time, parental lock/unlock. And call OnPolicyChanged.
  session_manager::SessionManager::Get()->AddObserver(this);
}

ScreenTimeController::~ScreenTimeController() {
  session_manager::SessionManager::Get()->RemoveObserver(this);
  SaveScreenTimeProgressBeforeExit();
}

base::TimeDelta ScreenTimeController::GetScreenTimeDuration() const {
  base::TimeDelta previous_duration = base::TimeDelta::FromMinutes(
      pref_service_->GetInteger(prefs::kScreenTimeMinutesUsed));
  if (current_screen_start_time_.is_null())
    return previous_duration;

  base::TimeDelta current_screen_duration =
      base::Time::Now() - current_screen_start_time_;
  return current_screen_duration + previous_duration;
}

void ScreenTimeController::CheckTimeLimit() {
  // Stop any currently running timer.
  ResetTimers();

  TimeLimitState state;
  // TODO(https://crbug.com/823536): Call time limit processer.
  // state = GetState(time_limit, GetScreenTimeDuration(),
  //     first_screen_start_time_, base::Time::Now());
  if (state.is_locked) {
    base::Time reset_time = base::Time();
    // TODO(https://crbug.com/823536): Get the expected reset time.
    // reset_time = getExpectedResetTime(time_limit, base::Time::Now());
    LockScreen(true /*force_lock_by_policy*/, reset_time);
  } else {
    if (state.active_policy == ActivePolicy::kNoActivePolicy)
      RefreshScreenLimit();
    LockScreen(false /*force_lock_by_policy*/, base::Time() /*come_back_time*/);

    if (state.is_time_usage_limit_enabled) {
      // Schedule notification based on the remaining screen time.
      const base::TimeDelta remaining_usage = state.remaining_usage;
      // TODO(wzang): Distinguish the notification type based on
      // |TimeLimitState|.
      const TimeLimitNotificationType notification_type = kScreenTime;

      if (remaining_usage >= kWarningNotificationTimeout) {
        warning_notification_timer_.Start(
            FROM_HERE, remaining_usage - kWarningNotificationTimeout,
            base::BindRepeating(&ScreenTimeController::ShowNotification,
                                base::Unretained(this), notification_type,
                                kWarningNotificationTimeout));
      }

      if (remaining_usage >= kExitNotificationTimeout) {
        exit_notification_timer_.Start(
            FROM_HERE, remaining_usage - kExitNotificationTimeout,
            base::BindRepeating(&ScreenTimeController::ShowNotification,
                                base::Unretained(this), notification_type,
                                kExitNotificationTimeout));
      }
    }
  }

  if (!state.next_state_change_time.is_null()) {
    next_state_timer_.Start(
        FROM_HERE, state.next_state_change_time - base::Time::Now(),
        base::BindRepeating(&ScreenTimeController::CheckTimeLimit,
                            base::Unretained(this)));
  }
}

void ScreenTimeController::LockScreen(bool force_lock_by_policy,
                                      base::Time come_back_time) {
  bool is_locked = session_manager::SessionManager::Get()->IsScreenLocked();
  // No-op if the screen is currently not locked and policy does not force the
  // lock.
  if (!is_locked && !force_lock_by_policy)
    return;

  // Request to show lock screen.
  if (!is_locked && force_lock_by_policy) {
    chromeos::DBusThreadManager::Get()
        ->GetSessionManagerClient()
        ->RequestLockScreen();
  }

  // TODO(https://crbug.com/823536): Mojo call to show/hide screen time message
  // in lock screen based on the policy.
}

void ScreenTimeController::ShowNotification(
    ScreenTimeController::TimeLimitNotificationType type,
    const base::TimeDelta& time_remaining) {
  const base::string16 title = l10n_util::GetStringUTF16(
      type == kScreenTime ? IDS_SCREEN_TIME_NOTIFICATION_TITLE
                          : IDS_BED_TIME_NOTIFICATION_TITLE);
  std::unique_ptr<message_center::Notification> notification =
      message_center::Notification::CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE, kTimeLimitNotificationId,
          title,
          ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_DURATION,
                                 ui::TimeFormat::LENGTH_LONG, time_remaining),
          gfx::Image(),
          l10n_util::GetStringUTF16(IDS_TIME_LIMIT_NOTIFICATION_DISPLAY_SOURCE),
          GURL(),
          message_center::NotifierId(
              message_center::NotifierId::SYSTEM_COMPONENT,
              kTimeLimitNotifierId),
          message_center::RichNotificationData(),
          new message_center::NotificationDelegate(),
          ash::kNotificationSupervisedUserIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);
  NotificationDisplayService::GetForProfile(
      Profile::FromBrowserContext(context_))
      ->Display(NotificationHandler::Type::TRANSIENT, *notification);
}

void ScreenTimeController::RefreshScreenLimit() {
  base::Time now = base::Time::Now();
  pref_service_->SetTime(prefs::kFirstScreenStartTime, now);
  pref_service_->SetTime(prefs::kCurrentScreenStartTime, now);
  pref_service_->SetInteger(prefs::kScreenTimeMinutesUsed, 0);
  pref_service_->CommitPendingWrite();

  first_screen_start_time_ = now;
  current_screen_start_time_ = now;
}

void ScreenTimeController::OnPolicyChanged() {
  CheckTimeLimit();
}

void ScreenTimeController::ResetTimers() {
  next_state_timer_.Stop();
  warning_notification_timer_.Stop();
  exit_notification_timer_.Stop();
  save_screen_time_timer_.Stop();
}

void ScreenTimeController::SaveScreenTimeProgressBeforeExit() {
  pref_service_->SetInteger(prefs::kScreenTimeMinutesUsed,
                            GetScreenTimeDuration().InMinutes());
  pref_service_->ClearPref(prefs::kCurrentScreenStartTime);
  pref_service_->CommitPendingWrite();
  current_screen_start_time_ = base::Time();
  ResetTimers();
}

void ScreenTimeController::SaveScreenTimeProgressPeriodically() {
  pref_service_->SetInteger(prefs::kScreenTimeMinutesUsed,
                            GetScreenTimeDuration().InMinutes());
  current_screen_start_time_ = base::Time::Now();
  pref_service_->SetTime(prefs::kCurrentScreenStartTime,
                         current_screen_start_time_);
  pref_service_->CommitPendingWrite();
}

void ScreenTimeController::OnSessionStateChanged() {
  session_manager::SessionState session_state =
      session_manager::SessionManager::Get()->session_state();
  if (session_state == session_manager::SessionState::LOCKED) {
    SaveScreenTimeProgressBeforeExit();
  } else if (session_state == session_manager::SessionState::ACTIVE) {
    base::Time now = base::Time::Now();
    const base::Time first_screen_start_time =
        pref_service_->GetTime(prefs::kFirstScreenStartTime);
    if (first_screen_start_time.is_null()) {
      pref_service_->SetTime(prefs::kFirstScreenStartTime, now);
      first_screen_start_time_ = now;
    } else {
      first_screen_start_time_ = first_screen_start_time;
    }

    const base::Time current_screen_start_time =
        pref_service_->GetTime(prefs::kCurrentScreenStartTime);
    if (!current_screen_start_time.is_null() &&
        current_screen_start_time < now &&
        (now - current_screen_start_time) <
            2 * kScreenTimeUsageUpdateFrequency) {
      current_screen_start_time_ = current_screen_start_time;
    } else {
      // If kCurrentScreenStartTime is not set or it's been too long since the
      // last update, set the time to now.
      current_screen_start_time_ = now;
    }
    pref_service_->SetTime(prefs::kCurrentScreenStartTime,
                           current_screen_start_time_);
    pref_service_->CommitPendingWrite();
    CheckTimeLimit();

    save_screen_time_timer_.Start(
        FROM_HERE, kScreenTimeUsageUpdateFrequency,
        base::BindRepeating(
            &ScreenTimeController::SaveScreenTimeProgressPeriodically,
            base::Unretained(this)));
  }
}

}  // namespace chromeos
