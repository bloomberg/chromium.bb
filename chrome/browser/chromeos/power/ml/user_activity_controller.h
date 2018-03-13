// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_ML_USER_ACTIVITY_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_POWER_ML_USER_ACTIVITY_CONTROLLER_H_

#include <memory>

#include "chrome/browser/chromeos/power/ml/idle_event_notifier.h"
#include "chrome/browser/chromeos/power/ml/user_activity_manager.h"
#include "chrome/browser/chromeos/power/ml/user_activity_ukm_logger_impl.h"

namespace chromeos {
namespace power {
namespace ml {

// This controller class sets up and destroys all the components associated with
// user activity logging (IdleEventNotifier, UserActivityUkmLogger and
// UserActivityManager).
class UserActivityController {
 public:
  UserActivityController();
  ~UserActivityController();

 private:
  std::unique_ptr<IdleEventNotifier> idle_event_notifier_;
  UserActivityUkmLoggerImpl user_activity_ukm_logger_;
  std::unique_ptr<UserActivityManager> user_activity_manager_;

  DISALLOW_COPY_AND_ASSIGN(UserActivityController);
};

}  // namespace ml
}  // namespace power
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_ML_USER_ACTIVITY_CONTROLLER_H_
