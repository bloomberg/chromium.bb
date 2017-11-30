// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/ml/idle_event_notifier.h"

#include "base/logging.h"
#include "base/time/default_clock.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"

namespace chromeos {
namespace power {
namespace ml {

IdleEventNotifier::ActivityData::ActivityData() = default;
IdleEventNotifier::ActivityData::ActivityData(base::Time last_activity_time,
                                              base::Time earliest_activity_time)
    : last_activity_time(last_activity_time),
      earliest_activity_time(earliest_activity_time) {}

IdleEventNotifier::ActivityData::ActivityData(const ActivityData& input_data) {
  last_activity_time = input_data.last_activity_time;
  earliest_activity_time = input_data.earliest_activity_time;
  last_user_activity_time = input_data.last_user_activity_time;
  last_mouse_time = input_data.last_mouse_time;
  last_key_time = input_data.last_key_time;
}

IdleEventNotifier::IdleEventNotifier(
    PowerManagerClient* power_manager_client,
    ui::UserActivityDetector* detector,
    viz::mojom::VideoDetectorObserverRequest request)
    : clock_(std::make_unique<base::DefaultClock>()),
      power_manager_client_observer_(this),
      user_activity_observer_(this),
      binding_(this, std::move(request)) {
  DCHECK(power_manager_client);
  power_manager_client_observer_.Add(power_manager_client);
  DCHECK(detector);
  user_activity_observer_.Add(detector);
}

IdleEventNotifier::~IdleEventNotifier() = default;

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
    UpdateActivityData(ActivityType::USER_OTHER);
    ResetIdleDelayTimer();
  }
}

void IdleEventNotifier::PowerChanged(
    const power_manager::PowerSupplyProperties& proto) {
  if (external_power_ != proto.external_power()) {
    external_power_ = proto.external_power();
    UpdateActivityData(ActivityType::USER_OTHER);
    ResetIdleDelayTimer();
  }
}

void IdleEventNotifier::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  if (idle_delay_timer_.IsRunning()) {
    // This means the user hasn't been idle for more than idle_delay. Regardless
    // of the reason of suspend, we won't be emitting any idle event now or
    // following SuspendDone. We stop the timer to prevent it from resuming in
    // SuspendDone.
    idle_delay_timer_.Stop();
  }
}

void IdleEventNotifier::SuspendDone(const base::TimeDelta& sleep_duration) {
  // TODO(jiameng): consider check sleep_duration to decide whether to log
  // event.
  DCHECK(!idle_delay_timer_.IsRunning());
  // SuspendDone is triggered by user opening the lid (or other user
  // activities).
  UpdateActivityData(ActivityType::USER_OTHER);
  ResetIdleDelayTimer();
}

void IdleEventNotifier::OnUserActivity(const ui::Event* event) {
  if (!event)
    return;
  // Get the type of activity first then reset timer.
  ActivityType type = ActivityType::USER_OTHER;
  if (event->IsKeyEvent()) {
    type = ActivityType::KEY;
  } else if (event->IsMouseEvent() || event->IsTouchEvent()) {
    type = ActivityType::MOUSE;
  }
  UpdateActivityData(type);
  ResetIdleDelayTimer();
}

void IdleEventNotifier::OnVideoActivityStarted() {
  video_playing_ = true;
  UpdateActivityData(ActivityType::VIDEO);
}

void IdleEventNotifier::OnVideoActivityEnded() {
  video_playing_ = false;
  UpdateActivityData(ActivityType::VIDEO);
  ResetIdleDelayTimer();
}

void IdleEventNotifier::ResetIdleDelayTimer() {
  // According to the policy, if there's video playing we should never dim or
  // turn off the screen (or transition to any other lower energy state).
  if (video_playing_)
    return;

  idle_delay_timer_.Start(FROM_HERE, idle_delay_, this,
                          &IdleEventNotifier::OnIdleDelayTimeout);
}

void IdleEventNotifier::OnIdleDelayTimeout() {
  if (video_playing_)
    return;

  for (auto& observer : observers_)
    observer.OnIdleEventObserved(data_);
  data_ = {};
}

void IdleEventNotifier::UpdateActivityData(ActivityType type) {
  base::Time now = clock_->Now();
  data_.last_activity_time = now;
  if (data_.earliest_activity_time == base::Time()) {
    data_.earliest_activity_time = now;
  }

  if (type == ActivityType::VIDEO)
    return;

  // All other activity is user-initiated.
  data_.last_user_activity_time = now;

  switch (type) {
    case ActivityType::KEY:
      data_.last_key_time = now;
      break;
    case ActivityType::MOUSE:
      data_.last_mouse_time = now;
      break;
    default:
      // We don't track other activity types.
      return;
  }
}

}  // namespace ml
}  // namespace power
}  // namespace chromeos
