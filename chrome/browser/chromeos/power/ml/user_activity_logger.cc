// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/ml/user_activity_logger.h"

#include <cmath>

#include "base/time/default_clock.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "chromeos/system/devicetype.h"

namespace chromeos {
namespace power {
namespace ml {

constexpr int kIdleDelaySeconds = 10;

UserActivityLogger::UserActivityLogger(
    UserActivityLoggerDelegate* delegate,
    IdleEventNotifier* idle_event_notifier,
    ui::UserActivityDetector* detector,
    chromeos::PowerManagerClient* power_manager_client,
    session_manager::SessionManager* session_manager,
    viz::mojom::VideoDetectorObserverRequest request,
    const chromeos::ChromeUserManager* user_manager)
    : clock_(std::make_unique<base::DefaultClock>()),
      logger_delegate_(delegate),
      idle_event_observer_(this),
      user_activity_observer_(this),
      power_manager_client_observer_(this),
      session_manager_observer_(this),
      session_manager_(session_manager),
      binding_(this, std::move(request)),
      user_manager_(user_manager),
      idle_delay_(base::TimeDelta::FromSeconds(kIdleDelaySeconds)),
      weak_ptr_factory_(this) {
  DCHECK(logger_delegate_);
  DCHECK(idle_event_notifier);
  idle_event_observer_.Add(idle_event_notifier);

  DCHECK(detector);
  user_activity_observer_.Add(detector);

  DCHECK(power_manager_client);
  power_manager_client_observer_.Add(power_manager_client);
  power_manager_client->RequestStatusUpdate();
  power_manager_client->GetSwitchStates(
      base::BindOnce(&UserActivityLogger::OnReceiveSwitchStates,
                     weak_ptr_factory_.GetWeakPtr()));
  power_manager_client->GetInactivityDelays(
      base::BindOnce(&UserActivityLogger::OnReceiveInactivityDelays,
                     weak_ptr_factory_.GetWeakPtr()));

  DCHECK(session_manager);
  session_manager_observer_.Add(session_manager);

  if (chromeos::GetDeviceType() == chromeos::DeviceType::kChromebook) {
    device_type_ = UserActivityEvent::Features::CHROMEBOOK;
  } else {
    device_type_ = UserActivityEvent::Features::UNKNOWN_DEVICE;
  }
}

UserActivityLogger::~UserActivityLogger() = default;

void UserActivityLogger::OnUserActivity(const ui::Event* /* event */) {
  MaybeLogEvent(UserActivityEvent::Event::REACTIVATE,
                UserActivityEvent::Event::USER_ACTIVITY);
}

void UserActivityLogger::LidEventReceived(
    chromeos::PowerManagerClient::LidState state,
    const base::TimeTicks& /* timestamp */) {
  lid_state_ = state;
  if (lid_state_ == chromeos::PowerManagerClient::LidState::CLOSED) {
    MaybeLogEvent(UserActivityEvent::Event::OFF,
                  UserActivityEvent::Event::LID_CLOSED);
  }
}

void UserActivityLogger::PowerChanged(
    const power_manager::PowerSupplyProperties& proto) {
  if (external_power_.has_value()) {
    bool power_source_changed = (*external_power_ != proto.external_power());

    // Only log when power source changed, don't care about percentage change.
    if (power_source_changed) {
      MaybeLogEvent(UserActivityEvent::Event::REACTIVATE,
                    UserActivityEvent::Event::POWER_CHANGED);
    }
  }
  external_power_ = proto.external_power();

  if (proto.has_battery_percent()) {
    battery_percent_ = proto.battery_percent();
  }
}

void UserActivityLogger::TabletModeEventReceived(
    chromeos::PowerManagerClient::TabletMode mode,
    const base::TimeTicks& /* timestamp */) {
  tablet_mode_ = mode;
}

void UserActivityLogger::ScreenIdleStateChanged(
    const power_manager::ScreenIdleState& proto) {
  if (proto.off()) {
    screen_idle_timer_.Start(
        FROM_HERE, idle_delay_,
        base::Bind(&UserActivityLogger::MaybeLogEvent, base::Unretained(this),
                   UserActivityEvent::Event::TIMEOUT,
                   UserActivityEvent::Event::SCREEN_OFF));
  }
}

void UserActivityLogger::SuspendDone(
    const base::TimeDelta& /* sleep_duration */) {
  MaybeLogEvent(UserActivityEvent::Event::TIMEOUT,
                UserActivityEvent::Event::IDLE_SLEEP);
}

void UserActivityLogger::InactivityDelaysChanged(
    const power_manager::PowerManagementPolicy::Delays& delays) {
  OnReceiveInactivityDelays(delays);
}

void UserActivityLogger::OnVideoActivityStarted() {
  MaybeLogEvent(UserActivityEvent::Event::REACTIVATE,
                UserActivityEvent::Event::VIDEO_ACTIVITY);
}

void UserActivityLogger::OnIdleEventObserved(
    const IdleEventNotifier::ActivityData& activity_data) {
  idle_event_start_ = clock_->Now();
  ExtractFeatures(activity_data);
}

void UserActivityLogger::OnSessionStateChanged() {
  DCHECK(session_manager_);
  const bool was_locked = screen_is_locked_;
  screen_is_locked_ = session_manager_->IsScreenLocked();
  if (!was_locked && screen_is_locked_) {
    MaybeLogEvent(UserActivityEvent::Event::OFF,
                  UserActivityEvent::Event::SCREEN_LOCK);
  }
}

void UserActivityLogger::OnReceiveSwitchStates(
    base::Optional<chromeos::PowerManagerClient::SwitchStates> switch_states) {
  if (switch_states.has_value()) {
    lid_state_ = switch_states->lid_state;
    tablet_mode_ = switch_states->tablet_mode;
  }
}

void UserActivityLogger::OnReceiveInactivityDelays(
    base::Optional<power_manager::PowerManagementPolicy::Delays> delays) {
  if (delays.has_value()) {
    screen_dim_delay_ =
        base::TimeDelta::FromMilliseconds(delays->screen_dim_ms());
    screen_off_delay_ =
        base::TimeDelta::FromMilliseconds(delays->screen_off_ms());
  }
}

void UserActivityLogger::ExtractFeatures(
    const IdleEventNotifier::ActivityData& activity_data) {
  features_.Clear();

  // Set transition times for dim and screen-off.
  if (!screen_dim_delay_.is_zero()) {
    features_.set_on_to_dim_sec(std::ceil(screen_dim_delay_.InSecondsF()));
  }
  if (!screen_off_delay_.is_zero()) {
    features_.set_dim_to_screen_off_sec(
        std::ceil((screen_off_delay_ - screen_dim_delay_).InSecondsF()));
  }

  // Set time related features.
  features_.set_last_activity_time_sec(
      (activity_data.last_activity_time -
       activity_data.last_activity_time.LocalMidnight())
          .InSeconds());
  if (!activity_data.last_user_activity_time.is_null()) {
    features_.set_last_user_activity_time_sec(
        (activity_data.last_user_activity_time -
         activity_data.last_user_activity_time.LocalMidnight())
            .InSeconds());
  }

  base::Time::Exploded exploded;
  activity_data.last_activity_time.LocalExplode(&exploded);
  features_.set_last_activity_day(
      static_cast<chromeos::power::ml::UserActivityEvent_Features_DayOfWeek>(
          exploded.day_of_week));

  if (!activity_data.last_mouse_time.is_null()) {
    features_.set_time_since_last_mouse_sec(
        (clock_->Now() - activity_data.last_mouse_time).InSeconds());
  }
  if (!activity_data.last_key_time.is_null()) {
    features_.set_time_since_last_key_sec(
        (clock_->Now() - activity_data.last_key_time).InSeconds());
  }

  features_.set_recent_time_active_sec(
      (activity_data.last_activity_time - activity_data.earliest_activity_time)
          .InSeconds());

  // Set device mode.
  if (lid_state_ == chromeos::PowerManagerClient::LidState::CLOSED) {
    features_.set_device_mode(UserActivityEvent::Features::CLOSED_LID);
  } else if (lid_state_ == chromeos::PowerManagerClient::LidState::OPEN) {
    if (tablet_mode_ == chromeos::PowerManagerClient::TabletMode::ON) {
      features_.set_device_mode(UserActivityEvent::Features::TABLET);
    } else {
      features_.set_device_mode(UserActivityEvent::Features::CLAMSHELL);
    }
  } else {
    features_.set_device_mode(UserActivityEvent::Features::UNKNOWN_MODE);
  }

  features_.set_device_type(device_type_);

  if (battery_percent_.has_value()) {
    features_.set_battery_percent(*battery_percent_);
  }
  if (external_power_.has_value()) {
    features_.set_on_battery(
        *external_power_ == power_manager::PowerSupplyProperties::DISCONNECTED);
  }

  if (user_manager_) {
    if (user_manager_->IsEnterpriseManaged()) {
      features_.set_device_management(UserActivityEvent::Features::MANAGED);
    } else {
      features_.set_device_management(UserActivityEvent::Features::UNMANAGED);
    }
  } else {
    features_.set_device_management(
        UserActivityEvent::Features::UNKNOWN_MANAGEMENT);
  }

  logger_delegate_->UpdateOpenTabsURLs();
}

void UserActivityLogger::MaybeLogEvent(
    UserActivityEvent::Event::Type type,
    UserActivityEvent::Event::Reason reason) {
  if (idle_event_start_.is_null())
    return;
  screen_idle_timer_.Stop();
  UserActivityEvent activity_event;

  UserActivityEvent::Event* event = activity_event.mutable_event();
  event->set_type(type);
  event->set_reason(reason);
  event->set_log_duration_sec((clock_->Now() - idle_event_start_).InSeconds());

  *activity_event.mutable_features() = features_;

  // Log to metrics.
  logger_delegate_->LogActivity(activity_event);
  idle_event_start_ = base::Time();
}

void UserActivityLogger::SetTaskRunnerForTesting(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    std::unique_ptr<base::Clock> test_clock) {
  screen_idle_timer_.SetTaskRunner(task_runner);
  clock_ = std::move(test_clock);
}

}  // namespace ml
}  // namespace power
}  // namespace chromeos
