// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_ANDROID_PREFETCH_BACKGROUND_TASK_ANDROID_H_
#define CHROME_BROWSER_OFFLINE_PAGES_ANDROID_PREFETCH_BACKGROUND_TASK_ANDROID_H_

#include "base/android/jni_android.h"
#include "components/offline_pages/core/prefetch/prefetch_background_task_handler.h"
#include "components/offline_pages/core/prefetch/prefetch_dispatcher.h"
#include "net/base/backoff_entry.h"

namespace offline_pages {
class PrefetchService;

// A task with a counterpart in Java for managing the background activity of the
// offline page prefetcher.  Schedules and listens for events about prefetching
// tasks.
class PrefetchBackgroundTaskAndroid
    : public PrefetchDispatcher::ScopedBackgroundTask {
 public:
  PrefetchBackgroundTaskAndroid(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_prefetch_background_task,
      PrefetchService* service);
  ~PrefetchBackgroundTaskAndroid() override;

  // Java hooks.
  bool OnStopTask(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& jcaller);
  void SetTaskReschedulingForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      jboolean reschedule,
      jboolean backoff);
  void SignalTaskFinishedForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller);

  // When this task completes, we tell the system whether the task should be
  // rescheduled with or without backoff.
  void SetNeedsReschedule(bool reschedule, bool backoff) override;
  bool needs_reschedule() { return needs_reschedule_; }

 private:
  bool task_killed_by_system_ = false;
  bool needs_reschedule_ = false;
  bool needs_backoff_ = false;

  // A pointer to the controlling |PrefetchBackgroundTask|.
  base::android::ScopedJavaGlobalRef<jobject> java_prefetch_background_task_;

  // The PrefetchService owns |this|, so a raw pointer is OK.
  PrefetchService* service_;
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_ANDROID_PREFETCH_BACKGROUND_TASK_ANDROID_H_
