// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OFFLINE_PAGES_BACKGROUND_SCHEDULER_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_OFFLINE_PAGES_BACKGROUND_SCHEDULER_BRIDGE_H_

#include "components/offline_pages/background/scheduler.h"

#include "base/android/jni_android.h"

namespace offline_pages {
namespace android {

// Bridge between C++ and Java for implementing background scheduler
// on Android.
class BackgroundSchedulerBridge : public Scheduler {
 public:
  // Scheduler implementation.
  void Schedule(const TriggerConditions& trigger_conditions) override;
  void BackupSchedule(const TriggerConditions& trigger_conditions,
                      long delay_in_seconds) override;
  void Unschedule() override;

 private:
  base::android::ScopedJavaLocalRef<jobject> CreateTriggerConditions(
      JNIEnv* env,
      bool require_power_connected,
      int minimum_battery_percentage,
      bool require_unmetered_network) const;
};

bool RegisterBackgroundSchedulerBridge(JNIEnv* env);

}  // namespace android
}  // namespace offline_pages

#endif  // CHROME_BROWSER_ANDROID_OFFLINE_PAGES_BACKGROUND_SCHEDULER_BRIDGE_H_
