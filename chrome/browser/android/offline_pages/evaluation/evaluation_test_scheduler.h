// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OFFLINE_PAGES_EVALUATION_EVALUATION_TEST_SCHEDULER_H_
#define CHROME_BROWSER_ANDROID_OFFLINE_PAGES_EVALUATION_EVALUATION_TEST_SCHEDULER_H_

#include "components/offline_pages/background/scheduler.h"

namespace offline_pages {
namespace android {

class EvaluationTestScheduler : public Scheduler {
 public:
  // Scheduler implementation.
  void Schedule(const TriggerConditions& trigger_conditions) override;
  void BackupSchedule(const TriggerConditions& trigger_conditions,
                      long delay_in_seconds) override;
  void Unschedule() override;

  // Callback used by user request.
  void ImmediateScheduleCallback(bool result);
};

}  // namespace android
}  // namespace offline_pages

#endif  // CHROME_BROWSER_ANDROID_OFFLINE_PAGES_EVALUATION_EVALUATION_TEST_SCHEDULER_H_
