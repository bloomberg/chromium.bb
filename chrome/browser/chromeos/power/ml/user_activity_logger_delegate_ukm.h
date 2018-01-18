// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_ML_USER_ACTIVITY_LOGGER_DELEGATE_UKM_H_
#define CHROME_BROWSER_CHROMEOS_POWER_ML_USER_ACTIVITY_LOGGER_DELEGATE_UKM_H_

#include <vector>

#include "base/macros.h"
#include "chrome/browser/chromeos/power/ml/user_activity_logger_delegate.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace chromeos {
namespace power {
namespace ml {

class UserActivityEvent;

class UserActivityLoggerDelegateUkm : public UserActivityLoggerDelegate {
 public:
  // Places |original_value| into buckets of size 5, i.e. if |original_value| is
  // in [0, 5), we map it 0; if it is in [5, 10), we map it to 5 etc.
  // |original_value| should be in the range of [0, 100].
  static int BucketEveryFivePercents(int original_value);

  // Bucket |timestamp_sec| such that
  // 1. if |timestamp_sec| < 60sec, return original value.
  // 2. if |timestamp_sec| < 5min, bucket to nearest 10sec.
  // 3. if |timestamp_sec| < 10min, bucket to nearest 20sec.
  // 4. if |timestamp_sec| >= 10min, cap it at 10min.
  // In all cases, the returned value is in seconds.
  static int ExponentiallyBucketTimestamp(int timestamp_sec);

  UserActivityLoggerDelegateUkm();
  ~UserActivityLoggerDelegateUkm() override;

  // chromeos::power::ml::UserActivityLoggerDelegate overrides:
  void UpdateOpenTabsURLs() override;
  void LogActivity(const UserActivityEvent& event) override;

 private:
  ukm::UkmRecorder* ukm_recorder_;  // not owned

  struct TabProperty {
    // Whether the tab is the selected one in its containing browser.
    bool is_active;
    // Whether the containing browser is in focus.
    bool is_browser_focused;
    // Whether the containing browser is visible.
    bool is_browser_visible;
    // Whether the containing browser is the topmost one on the screen.
    bool is_topmost_browser;
  };

  // Source IDs of open tabs' URLs.
  std::map<ukm::SourceId, TabProperty> source_ids_;

  DISALLOW_COPY_AND_ASSIGN(UserActivityLoggerDelegateUkm);
};

}  // namespace ml
}  // namespace power
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_ML_USER_ACTIVITY_LOGGER_DELEGATE_UKM_H_
