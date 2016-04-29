// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OFFLINE_PAGES_BACKGROUND_SCHEDULER_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_OFFLINE_PAGES_BACKGROUND_SCHEDULER_BRIDGE_H_

#include "components/offline_pages/background/scheduler.h"

#include "base/android/jni_android.h"

namespace content {
class BrowserContext;
}

namespace offline_pages {
namespace android {

// Bridge between C++ and Java for implementing background scheduler
// on Android.
class BackgroundSchedulerBridge : Scheduler {
 public:
  BackgroundSchedulerBridge();
  ~BackgroundSchedulerBridge() override;

  // Scheduler implementation.
  void Schedule(const TriggerCondition& trigger_condition) override;
  void Unschedule() override;
};

bool RegisterBackgroundSchedulerBridge(JNIEnv* env);

}  // namespace android
}  // namespace offline_pages

#endif  // CHROME_BROWSER_ANDROID_OFFLINE_PAGES_BACKGROUND_SCHEDULER_BRIDGE_H_

