// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_ML_USER_ACTIVITY_UKM_LOGGER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_POWER_ML_USER_ACTIVITY_UKM_LOGGER_IMPL_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/power/ml/user_activity_ukm_logger.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace chromeos {
namespace power {
namespace ml {

class UserActivityEvent;

class UserActivityUkmLoggerImpl : public UserActivityUkmLogger {
 public:
  UserActivityUkmLoggerImpl();
  ~UserActivityUkmLoggerImpl() override;

  // chromeos::power::ml::UserActivityUkmLogger overrides:
  void LogActivity(const UserActivityEvent& event) override;

 private:
  friend class UserActivityUkmLoggerTest;

  ukm::UkmRecorder* ukm_recorder_;  // not owned

  // This ID is incremented each time a UserActivity is logged to UKM.
  // Event index starts from 1, and resets when a new session starts.
  int next_sequence_id_ = 1;

  DISALLOW_COPY_AND_ASSIGN(UserActivityUkmLoggerImpl);
};

}  // namespace ml
}  // namespace power
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_ML_USER_ACTIVITY_UKM_LOGGER_IMPL_H_
