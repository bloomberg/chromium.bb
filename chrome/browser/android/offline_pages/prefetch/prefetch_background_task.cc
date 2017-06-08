// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/prefetch/prefetch_background_task.h"

#include "base/time/time.h"
#include "chrome/browser/offline_pages/prefetch/prefetch_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/common/pref_names.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "jni/PrefetchBackgroundTask_jni.h"
#include "net/base/backoff_entry_serializer.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace offline_pages {

const net::BackoffEntry::Policy kBackoffPolicy = {
    0,                 // Number of initial errors to ignore without backoff.
    30 * 1000,         // Initial delay for backoff in ms: 30 seconds.
    2,                 // Factor to multiply for exponential backoff.
    0.33,              // Fuzzing percentage.
    24 * 3600 * 1000,  // Maximum time to delay requests in ms: 1 day.
    -1,                // Don't discard entry even if unused.
    false              // Don't use initial delay unless the last was an error.
};

namespace prefetch {

// JNI call to start request processing in scheduled mode.
static jboolean StartPrefetchTask(JNIEnv* env,
                                  const JavaParamRef<jobject>& jcaller,
                                  const JavaParamRef<jobject>& jprofile) {
  if (!IsPrefetchingOfflinePagesEnabled())
    return false;

  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  DCHECK(profile);

  PrefetchService* prefetch_service =
      PrefetchServiceFactory::GetForBrowserContext(profile);
  if (!prefetch_service)
    return false;

  prefetch_service->GetPrefetchDispatcher()->BeginBackgroundTask(
      base::MakeUnique<PrefetchBackgroundTask>(env, jcaller, prefetch_service,
                                               profile));
  return false;  // true;
}

}  // namespace prefetch

void RegisterPrefetchBackgroundTaskPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kOfflinePrefetchBackoff);
}

// static
void PrefetchBackgroundTask::Schedule(int additional_delay_seconds) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return prefetch::Java_PrefetchBackgroundTask_scheduleTask(
      env, additional_delay_seconds);
}

// static
void PrefetchBackgroundTask::Cancel() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return prefetch::Java_PrefetchBackgroundTask_cancelTask(env);
}

PrefetchBackgroundTask::PrefetchBackgroundTask(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_prefetch_background_task,
    PrefetchService* service,
    Profile* profile)
    : java_prefetch_background_task_(java_prefetch_background_task),
      service_(service),
      profile_(profile) {
  // Give the Java side a pointer to the new background task object.
  prefetch::Java_PrefetchBackgroundTask_setNativeTask(
      env, java_prefetch_background_task_, reinterpret_cast<jlong>(this));

  SetupBackOff();
}

PrefetchBackgroundTask::~PrefetchBackgroundTask() {
  JNIEnv* env = base::android::AttachCurrentThread();
  prefetch::Java_PrefetchBackgroundTask_doneProcessing(
      env, java_prefetch_background_task_.obj(), needs_reschedule_);

  if (needs_reschedule_) {
    // If the task is killed due to the system, it should be rescheduled without
    // backoff even when it is in effect because we want to rerun the task asap.
    Schedule(task_killed_by_system_ ? 0 : GetAdditionalBackoffSeconds());
  }
}

void PrefetchBackgroundTask::SetupBackOff() {
  const base::ListValue* value =
      profile_->GetPrefs()->GetList(prefs::kOfflinePrefetchBackoff);
  if (value) {
    backoff_ = net::BackoffEntrySerializer::DeserializeFromValue(
        *value, &kBackoffPolicy, nullptr, base::Time::Now());
  }

  if (!backoff_)
    backoff_ = base::MakeUnique<net::BackoffEntry>(&kBackoffPolicy);
}

bool PrefetchBackgroundTask::OnStopTask(JNIEnv* env,
                                        const JavaParamRef<jobject>& jcaller) {
  task_killed_by_system_ = true;
  needs_reschedule_ = true;
  service_->GetPrefetchDispatcher()->StopBackgroundTask();
  return false;
}

void PrefetchBackgroundTask::SetTaskReschedulingForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jboolean reschedule,
    jboolean backoff) {
  SetNeedsReschedule(static_cast<bool>(reschedule), static_cast<bool>(backoff));
}

void PrefetchBackgroundTask::SignalTaskFinishedForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  service_->GetPrefetchDispatcher()->RequestFinishBackgroundTaskForTest();
}

void PrefetchBackgroundTask::SetNeedsReschedule(bool reschedule, bool backoff) {
  needs_reschedule_ = reschedule;

  if (reschedule && backoff)
    backoff_->InformOfRequest(false);
  else
    backoff_->Reset();

  std::unique_ptr<base::Value> value =
      net::BackoffEntrySerializer::SerializeToValue(*backoff_,
                                                    base::Time::Now());
  profile_->GetPrefs()->Set(prefs::kOfflinePrefetchBackoff, *value);
}

int PrefetchBackgroundTask::GetAdditionalBackoffSeconds() const {
  return static_cast<int>(backoff_->GetTimeUntilRelease().InSeconds());
}

bool RegisterPrefetchBackgroundTask(JNIEnv* env) {
  return prefetch::RegisterNativesImpl(env);
}

}  // namespace offline_pages
