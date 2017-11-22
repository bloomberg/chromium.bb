// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_ML_USER_ACTIVITY_LOGGER_H_
#define CHROME_BROWSER_CHROMEOS_POWER_ML_USER_ACTIVITY_LOGGER_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/power/ml/idle_event_notifier.h"
#include "chrome/browser/chromeos/power/ml/user_activity_logger_delegate.h"
#include "ui/base/user_activity/user_activity_observer.h"

namespace chromeos {
namespace power {
namespace ml {

// Logs user activity after an idle event is observed.
// TODO(renjieliu): Add power-related activity as well.
class UserActivityLogger : public ui::UserActivityObserver,
                           public IdleEventNotifier::Observer {
 public:
  explicit UserActivityLogger(UserActivityLoggerDelegate* delegate);
  ~UserActivityLogger() override;

 private:
  friend class UserActivityLoggerTest;

  // ui::UserActivityObserver overrides.
  void OnUserActivity(const ui::Event* event) override;

  // IdleEventNotifier::Observer overrides.
  void OnIdleEventObserved(
      const IdleEventNotifier::ActivityData& data) override;

  // Flag indicating whether an idle event has been observed.
  bool idle_event_observed_ = false;

  // UserActivity data derived from idle event.
  IdleEventNotifier::ActivityData activity_data_;

  // Logger delegate.
  UserActivityLoggerDelegate* const logger_delegate_;

  DISALLOW_COPY_AND_ASSIGN(UserActivityLogger);
};

}  // namespace ml
}  // namespace power
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_ML_USER_ACTIVITY_LOGGER_H_
