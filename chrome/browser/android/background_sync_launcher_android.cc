// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/background_sync_launcher_android.h"

#include <utility>

#include "base/android/callback_android.h"
#include "base/barrier_closure.h"
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
void BackgroundSyncLauncherAndroid::LaunchBrowserIfStopped(
    bool launch_when_next_online,
    int64_t min_delay_ms) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Get()->LaunchBrowserIfStoppedImpl(launch_when_next_online, min_delay_ms);
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

void BackgroundSyncLauncherAndroid::LaunchBrowserIfStoppedImpl(
    bool launch_when_next_online,
    int64_t min_delay_ms) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  JNIEnv* env = base::android::AttachCurrentThread();

  if (!base::FeatureList::IsEnabled(
          chrome::android::kBackgroundTaskSchedulerForBackgroundSync)) {
    Java_BackgroundSyncLauncher_launchBrowserIfStopped(
        env, java_gcm_network_manager_launcher_, launch_when_next_online,
        min_delay_ms);
    return;
  }

  Java_BackgroundSyncBackgroundTaskScheduler_launchBrowserIfStopped(
      env, java_background_sync_background_task_scheduler_launcher_,
      launch_when_next_online, min_delay_ms);
}

void BackgroundSyncLauncherAndroid::FireBackgroundSyncEvents(
    const base::android::JavaParamRef<jobject>& j_runnable) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto* profile = ProfileManager::GetLastUsedProfile();
  DCHECK(profile);

  int num_partitions = 0;
  content::BrowserContext::ForEachStoragePartition(
      profile, base::BindRepeating(
                   [](int* num_partitions,
                      content::StoragePartition* storage_partition) {
                     (*num_partitions)++;
                   },
                   &num_partitions));

  // This class is a singleton, which is only destructed on program exit.
  // Therefore, use of base::Unretained(this) is safe.
  base::RepeatingClosure done_closure = base::BarrierClosure(
      num_partitions,
      base::BindOnce(base::android::RunRunnableAndroid,
                     base::android::ScopedJavaGlobalRef<jobject>(j_runnable)));

  content::BrowserContext::ForEachStoragePartition(
      profile,
      base::BindRepeating(&BackgroundSyncLauncherAndroid::
                              FireBackgroundSyncEventsForStoragePartition,
                          base::Unretained(this), done_closure));
}

void BackgroundSyncLauncherAndroid::FireBackgroundSyncEventsForStoragePartition(
    base::OnceClosure done_closure,
    content::StoragePartition* storage_partition) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  content::BackgroundSyncContext* sync_context =
      storage_partition->GetBackgroundSyncContext();
  if (!sync_context) {
    std::move(done_closure).Run();
    return;
  }

  sync_context->FireBackgroundSyncEventsForStoragePartition(
      storage_partition, std::move(done_closure));
}

void BackgroundSyncLauncherAndroid::OnFiredBackgroundSyncEvents(
    base::android::ScopedJavaGlobalRef<jobject> j_runnable) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::android::RunRunnableAndroid(j_runnable);
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
