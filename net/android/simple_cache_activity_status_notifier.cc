// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/android/simple_cache_activity_status_notifier.h"

#include "base/android/jni_android.h"
#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/task_runner.h"
#include "jni/SimpleCacheActivityStatusNotifier_jni.h"

namespace net {

SimpleCacheActivityStatusNotifier::SimpleCacheActivityStatusNotifier(
    const ActivityStatusChangedCallback& notify_callback)
    : notify_callback_(notify_callback) {
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);
  java_obj_.Reset(
      Java_SimpleCacheActivityStatusNotifier_newInstance(
          env, reinterpret_cast<jint>(this)));
}

SimpleCacheActivityStatusNotifier::~SimpleCacheActivityStatusNotifier() {
  DCHECK(thread_checker_.CalledOnValidThread());
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);
  Java_SimpleCacheActivityStatusNotifier_prepareToBeDestroyed(
      env, java_obj_.obj());
}

void SimpleCacheActivityStatusNotifier::NotifyActivityStatusChanged(
    JNIEnv* env,
    jobject obj,
    jint j_new_activity_status) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ActivityStatus new_activity_status =
      static_cast<ActivityStatus>(j_new_activity_status);
  DCHECK(!notify_callback_.is_null());
  notify_callback_.Run(new_activity_status);
}

// static
bool SimpleCacheActivityStatusNotifier::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace net
