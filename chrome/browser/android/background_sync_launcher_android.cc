// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/background_sync_launcher_android.h"

#include <utility>

#include "base/android/callback_android.h"
#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/background_sync_context.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "jni/BackgroundSyncBackgroundTaskScheduler_jni.h"
#include "jni/BackgroundSyncBackgroundTask_jni.h"
#include "jni/BackgroundSyncLauncher_jni.h"

using content::BrowserThread;

namespace {
base::LazyInstance<BackgroundSyncLauncherAndroid>::DestructorAtExit
    g_background_sync_launcher = LAZY_INSTANCE_INITIALIZER;

// Disables the Play Services version check for testing on Chromium build bots.
// TODO(iclelland): Remove this once the bots have their play services package
// updated before every test run. (https://crbug.com/514449)
bool disable_play_services_version_check_for_tests = false;

}  // namespace

// static
void JNI_BackgroundSyncBackgroundTask_FireBackgroundSyncEvents(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_runnable) {
  if (!base::FeatureList::IsEnabled(
          chrome::android::kBackgroundTaskSchedulerForBackgroundSync)) {
    return;
  }

  BackgroundSyncLauncherAndroid::Get()->FireBackgroundSyncEvents(j_runnable);
}

// static
BackgroundSyncLauncherAndroid* BackgroundSyncLauncherAndroid::Get() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  return g_background_sync_launcher.Pointer();
}

// static
void BackgroundSyncLauncherAndroid::LaunchBrowserIfStopped() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Get()->LaunchBrowserIfStoppedImpl();
}

// static
void BackgroundSyncLauncherAndroid::SetPlayServicesVersionCheckDisabledForTests(
    bool disabled) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  disable_play_services_version_check_for_tests = disabled;
}

// static
bool BackgroundSyncLauncherAndroid::ShouldDisableBackgroundSync() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (disable_play_services_version_check_for_tests)
    return false;
  return Java_BackgroundSyncLauncher_shouldDisableBackgroundSync(
      base::android::AttachCurrentThread());
}

void BackgroundSyncLauncherAndroid::LaunchBrowserIfStoppedImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto* profile = ProfileManager::GetLastUsedProfile();
  DCHECK(profile);

  content::BackgroundSyncContext::GetSoonestWakeupDeltaAcrossPartitions(
      profile, base::BindOnce(
                   &BackgroundSyncLauncherAndroid::LaunchBrowserWithWakeupDelta,
                   base::Unretained(this)));
}

void BackgroundSyncLauncherAndroid::LaunchBrowserWithWakeupDelta(
    base::TimeDelta soonest_wakeup_delta) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  JNIEnv* env = base::android::AttachCurrentThread();
  int64_t min_delay_ms = soonest_wakeup_delta.InMilliseconds();

  if (!base::FeatureList::IsEnabled(
          chrome::android::kBackgroundTaskSchedulerForBackgroundSync)) {
    Java_BackgroundSyncLauncher_launchBrowserIfStopped(
        env, java_gcm_network_manager_launcher_,
        /* shouldLaunch= */ !soonest_wakeup_delta.is_max(), min_delay_ms);
    return;
  }

  Java_BackgroundSyncBackgroundTaskScheduler_launchBrowserIfStopped(
      env, java_background_sync_background_task_scheduler_launcher_,
      !soonest_wakeup_delta.is_max(), min_delay_ms);
}

void BackgroundSyncLauncherAndroid::FireBackgroundSyncEvents(
    const base::android::JavaParamRef<jobject>& j_runnable) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto* profile = ProfileManager::GetLastUsedProfile();
  DCHECK(profile);

  content::BackgroundSyncContext::FireBackgroundSyncEventsAcrossPartitions(
      profile, j_runnable);
}


BackgroundSyncLauncherAndroid::BackgroundSyncLauncherAndroid() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  JNIEnv* env = base::android::AttachCurrentThread();

  if (!base::FeatureList::IsEnabled(
          chrome::android::kBackgroundTaskSchedulerForBackgroundSync)) {
    java_gcm_network_manager_launcher_.Reset(
        Java_BackgroundSyncLauncher_create(env));
    return;
  }

  java_background_sync_background_task_scheduler_launcher_.Reset(
      Java_BackgroundSyncBackgroundTaskScheduler_getInstance(env));
}

BackgroundSyncLauncherAndroid::~BackgroundSyncLauncherAndroid() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (base::FeatureList::IsEnabled(
          chrome::android::kBackgroundTaskSchedulerForBackgroundSync)) {
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_BackgroundSyncLauncher_destroy(env, java_gcm_network_manager_launcher_);
}
