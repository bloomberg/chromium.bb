// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/component_updater/background_task_update_scheduler.h"

#include "jni/UpdateScheduler_jni.h"

namespace component_updater {

// static
bool BackgroundTaskUpdateScheduler::IsAvailable() {
  return Java_UpdateScheduler_isAvailable(base::android::AttachCurrentThread());
}

BackgroundTaskUpdateScheduler::BackgroundTaskUpdateScheduler() {
  DCHECK(IsAvailable());
  JNIEnv* env = base::android::AttachCurrentThread();
  j_update_scheduler_.Reset(Java_UpdateScheduler_getInstance(env));
  Java_UpdateScheduler_setNativeScheduler(env, j_update_scheduler_,
                                          reinterpret_cast<intptr_t>(this));
}

BackgroundTaskUpdateScheduler::~BackgroundTaskUpdateScheduler() = default;

void BackgroundTaskUpdateScheduler::Schedule(
    const base::TimeDelta& initial_delay,
    const base::TimeDelta& delay,
    const UserTask& user_task,
    const OnStopTaskCallback& on_stop) {
  user_task_ = user_task;
  on_stop_ = on_stop;
  Java_UpdateScheduler_schedule(
      base::android::AttachCurrentThread(), j_update_scheduler_,
      initial_delay.InMilliseconds(), delay.InMilliseconds());
}

void BackgroundTaskUpdateScheduler::Stop() {
  Java_UpdateScheduler_cancelTask(base::android::AttachCurrentThread(),
                                  j_update_scheduler_);
}

void BackgroundTaskUpdateScheduler::OnStartTask(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  user_task_.Run(base::BindOnce(&Java_UpdateScheduler_finishTask,
                                base::Unretained(env), j_update_scheduler_));
}

void BackgroundTaskUpdateScheduler::OnStopTask(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  DCHECK(on_stop_);
  on_stop_.Run();
}

}  // namespace component_updater
