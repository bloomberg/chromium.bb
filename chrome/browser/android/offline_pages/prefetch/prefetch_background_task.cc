// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/prefetch/prefetch_background_task.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/offline_pages/content/prefetch_service_factory.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"
#include "content/public/browser/browser_context.h"
#include "jni/PrefetchBackgroundTask_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace offline_pages {
namespace prefetch {

// JNI call to start request processing in scheduled mode.
static jboolean StartPrefetchTask(JNIEnv* env,
                                  const JavaParamRef<jobject>& jcaller,
                                  const JavaParamRef<jobject>& jprofile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  DCHECK(profile);

  PrefetchService* prefetch_service =
      PrefetchServiceFactory::GetForBrowserContext(profile);
  if (!prefetch_service)
    return false;

  prefetch_service->GetDispatcher()->BeginBackgroundTask(
      base::MakeUnique<PrefetchBackgroundTask>(env, jcaller, prefetch_service));
  return true;
}

}  // namespace prefetch

// static
void PrefetchBackgroundTask::Schedule() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return prefetch::Java_PrefetchBackgroundTask_scheduleTask(env);
}

// static
void PrefetchBackgroundTask::Cancel() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return prefetch::Java_PrefetchBackgroundTask_cancelTask(env);
}

PrefetchBackgroundTask::PrefetchBackgroundTask(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_prefetch_background_task,
    PrefetchService* service)
    : java_prefetch_background_task_(java_prefetch_background_task),
      service_(service) {
  // Give the Java side a pointer to the new background task object.
  prefetch::Java_PrefetchBackgroundTask_setNativeTask(
      env, java_prefetch_background_task_, reinterpret_cast<jlong>(this));
}

PrefetchBackgroundTask::~PrefetchBackgroundTask() {
  JNIEnv* env = base::android::AttachCurrentThread();
  prefetch::Java_PrefetchBackgroundTask_doneProcessing(
      env, java_prefetch_background_task_.obj(), needs_reschedule_);
}

bool PrefetchBackgroundTask::OnStopTask(JNIEnv* env,
                                        const JavaParamRef<jobject>& jcaller) {
  DCHECK(jcaller.obj() == java_prefetch_background_task_.obj());
  service_->GetDispatcher()->StopBackgroundTask(this);
  return needs_reschedule_;
}

void PrefetchBackgroundTask::SetNeedsReschedule(bool reschedule) {
  needs_reschedule_ = reschedule;
}

bool RegisterPrefetchBackgroundTask(JNIEnv* env) {
  return prefetch::RegisterNativesImpl(env);
}

}  // namespace offline_pages
