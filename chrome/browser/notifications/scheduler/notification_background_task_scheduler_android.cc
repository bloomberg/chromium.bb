// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/notification_background_task_scheduler_android.h"

#include "base/android/jni_android.h"
#include "base/logging.h"
#include "jni/NotificationSchedulerTask_jni.h"

// static
void JNI_NotificationSchedulerTask_OnStartTask(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jobject>& profile,
    const base::android::JavaParamRef<jobject>& callback) {
  Java_org_chromium_chrome_browser_notifications_scheduler_NotificationSchedulerTask_nativeOnStartTask(
      env, jcaller, profile, callback);
}

// static
jboolean JNI_NotificationSchedulerTask_OnStopTask(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jobject>& profile) {
  return Java_org_chromium_chrome_browser_notifications_scheduler_NotificationSchedulerTask_nativeOnStopTask(
      env, jcaller, profile);
}

NotificationBackgroundTaskSchedulerAndroid::
    NotificationBackgroundTaskSchedulerAndroid() = default;

NotificationBackgroundTaskSchedulerAndroid::
    ~NotificationBackgroundTaskSchedulerAndroid() = default;

void NotificationBackgroundTaskSchedulerAndroid::Schedule(
    notifications::SchedulerTaskTime scheduler_task_time,
    base::TimeDelta window_start,
    base::TimeDelta window_end) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_NotificationSchedulerTask_schedule(
      env, static_cast<jint>(scheduler_task_time),
      base::saturated_cast<jlong>(window_start.InMilliseconds()),
      base::saturated_cast<jlong>(window_end.InMilliseconds()));
}
