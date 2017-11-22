// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/ml/idle_event_notifier.h"

#include "base/logging.h"
#include "base/time/default_clock.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"

namespace chromeos {
namespace power {
namespace ml {

IdleEventNotifier::IdleEventNotifier(const base::TimeDelta& idle_delay)
    : idle_delay_(idle_delay), clock_(std::make_unique<base::DefaultClock>()) {
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(
      this);
}

IdleEventNotifier::~IdleEventNotifier() {
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(
      this);
}

void IdleEventNotifier::SetClockForTesting(
    std::unique_ptr<base::Clock> test_clock) {
  clock_ = std::move(test_clock);
}

void IdleEventNotifier::AddObserver(Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void IdleEventNotifier::RemoveObserver(Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void IdleEventNotifier::LidEventReceived(
    chromeos::PowerManagerClient::LidState state,
    const base::TimeTicks& /* timestamp */) {
  // Ignore lid-close event, as we will observe suspend signal.
  if (state == chromeos::PowerManagerClient::LidState::OPEN) {
    last_activity_time_ = clock_->Now();
    ResetIdleDelayTimer();
  }
}

void IdleEventNotifier::PowerChanged(
    const power_manager::PowerSupplyProperties& proto) {
  if (external_power_ != proto.external_power()) {
    external_power_ = proto.external_power();
    last_activity_time_ = clock_->Now();
    // Power change is a user activity, so reset timer.
    ResetIdleDelayTimer();
  }
}

void IdleEventNotifier::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  if (idle_delay_timer_.IsRunning()) {
    // This means the user hasn't been idle for more than kModelStartDelaySec.
    // Regardless of the reason of suspend, we won't be emitting any idle event
    // now or following SuspendDone.
    // We stop the timer to prevent it from resuming in SuspendDone.
    idle_delay_timer_.Stop();
  }
}

void IdleEventNotifier::SuspendDone(const base::TimeDelta& sleep_duration) {
  // TODO(jiameng): consider check sleep_duration to decide whether to log
  // event.
  DCHECK(!idle_delay_timer_.IsRunning());
  last_activity_time_ = clock_->Now();
  ResetIdleDelayTimer();
}

void IdleEventNotifier::ResetIdleDelayTimer() {
  idle_delay_timer_.Start(FROM_HERE, idle_delay_, this,
                          &IdleEventNotifier::OnIdleDelayTimeout);
}

void IdleEventNotifier::OnIdleDelayTimeout() {
  ActivityData activity_data;
  activity_data.last_activity_time = last_activity_time_;
  for (auto& observer : observers_)
    observer.OnIdleEventObserved(activity_data);
}

}  // namespace ml
}  // namespace power
}  // namespace chromeos
