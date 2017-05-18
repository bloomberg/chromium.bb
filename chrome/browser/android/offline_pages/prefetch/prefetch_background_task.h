// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OFFLINE_PAGES_PREFETCH_PREFETCH_BACKGROUND_TASK_H_
#define CHROME_BROWSER_ANDROID_OFFLINE_PAGES_PREFETCH_PREFETCH_BACKGROUND_TASK_H_

#include "base/android/jni_android.h"
#include "components/offline_pages/core/prefetch/prefetch_dispatcher.h"

namespace offline_pages {
class PrefetchService;

// A task with a counterpart in Java for managing the background activity of the
// offline page prefetcher.  Schedules and listens for events about prefetching
// tasks.
class PrefetchBackgroundTask : public PrefetchDispatcher::ScopedBackgroundTask {
 public:
  PrefetchBackgroundTask(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_prefetch_background_task,
      PrefetchService* service);
  ~PrefetchBackgroundTask() override;

  // API for interacting with BackgroundTaskScheduler from native.
  // Schedules the default 'NWake' prefetching task.
  static void Schedule();

  // Cancels the default 'NWake' prefetching task.
  static void Cancel();

  // Java hooks.
  bool OnStopTask(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& jcaller);

  // When this task completes, we tell the system whether the task should be
  // rescheduled with the same parameters as last time.
  void SetNeedsReschedule(bool reschedule) override;
  bool needs_reschedule() { return needs_reschedule_; }

 private:
  bool needs_reschedule_ = true;

  // A pointer to the controlling |PrefetchBackgroundTask|.
  base::android::ScopedJavaGlobalRef<jobject> java_prefetch_background_task_;

  // The PrefetchService owns |this|, so a raw pointer is OK.
  PrefetchService* service_;
};

bool RegisterPrefetchBackgroundTask(JNIEnv* env);

}  // namespace offline_pages

#endif  // CHROME_BROWSER_ANDROID_OFFLINE_PAGES_PREFETCH_PREFETCH_BACKGROUND_TASK_H_
