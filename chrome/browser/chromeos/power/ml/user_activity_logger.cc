// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/ml/user_activity_logger.h"

#include "base/timer/timer.h"
#include "chrome/browser/chromeos/power/ml/user_activity_event.pb.h"
#include "ui/base/user_activity/user_activity_observer.h"

namespace chromeos {
namespace power {
namespace ml {

UserActivityLogger::UserActivityLogger(
    UserActivityLoggerDelegate* const delegate)
    : logger_delegate_(delegate) {}

UserActivityLogger::~UserActivityLogger() = default;

// Only log when we observe an idle event.
void UserActivityLogger::OnUserActivity(const ui::Event* /* event */) {
  if (!idle_event_observed_)
    return;
  UserActivityEvent activity_event;

  UserActivityEvent::Event* event = activity_event.mutable_event();
  event->set_type(UserActivityEvent::Event::REACTIVATE);
  event->set_reason(UserActivityEvent::Event::USER_ACTIVITY);

  UserActivityEvent::Features* features = activity_event.mutable_features();
  features->set_last_activity_time_sec(
      (activity_data_.last_activity_time -
       activity_data_.last_activity_time.LocalMidnight())
          .InSeconds());

  // Log to metrics.
  logger_delegate_->LogActivity(activity_event);
  idle_event_observed_ = false;
}

void UserActivityLogger::OnIdleEventObserved(
    const IdleEventNotifier::ActivityData& activity_data) {
  idle_event_observed_ = true;
  activity_data_ = activity_data;
}

}  // namespace ml
}  // namespace power
}  // namespace chromeos
