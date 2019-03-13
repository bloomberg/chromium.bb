// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_launcher.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/public/browser/background_sync_context.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#if defined(OS_ANDROID)
#include "base/android/callback_android.h"
#include "base/barrier_closure.h"
#endif

namespace content {

namespace {

base::LazyInstance<BackgroundSyncLauncher>::DestructorAtExit
    g_background_sync_launcher = LAZY_INSTANCE_INITIALIZER;

#if defined(OS_ANDROID)
unsigned int GetStoragePartitionCount(BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(browser_context);

  int num_partitions = 0;
  BrowserContext::ForEachStoragePartition(
      browser_context,
      base::BindRepeating(
          [](int* num_partitions, StoragePartition* storage_partition) {
            (*num_partitions)++;
          },
          &num_partitions));

  // It's valid for a profile to not have any storage partitions. This DCHECK
  // is to ensure that we're not waking up Chrome for no reason, because that's
  // expensive and unnecessary.
  DCHECK(num_partitions);

  return num_partitions;
}
#endif

}  // namespace

// static
BackgroundSyncLauncher* BackgroundSyncLauncher::Get() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  return g_background_sync_launcher.Pointer();
}

// static
void BackgroundSyncLauncher::GetSoonestWakeupDelta(
    BrowserContext* browser_context,
    base::OnceCallback<void(base::TimeDelta)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::TimeDelta soonest_wakeup_delta = base::TimeDelta::Max();
  BrowserContext::ForEachStoragePartition(
      browser_context, base::BindRepeating(
                           [](base::TimeDelta* soonest_wakeup_delta,
                              StoragePartition* storage_partition) {
                             BackgroundSyncContext* sync_context =
                                 storage_partition->GetBackgroundSyncContext();
                             DCHECK(sync_context);

                             base::TimeDelta wakeup_delta =
                                 sync_context->GetSoonestWakeupDelta();
                             if (wakeup_delta < *soonest_wakeup_delta)
                               *soonest_wakeup_delta = wakeup_delta;
                           },
                           &soonest_wakeup_delta));
  std::move(callback).Run(soonest_wakeup_delta);
}

// static
#if defined(OS_ANDROID)
void BackgroundSyncLauncher::FireBackgroundSyncEvents(
    BrowserContext* browser_context,
    const base::android::JavaParamRef<jobject>& j_runnable) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(browser_context);

  base::RepeatingClosure done_closure = base::BarrierClosure(
      GetStoragePartitionCount(browser_context),
      base::BindOnce(base::android::RunRunnableAndroid,
                     base::android::ScopedJavaGlobalRef<jobject>(j_runnable)));

  BrowserContext::ForEachStoragePartition(
      browser_context,
      base::BindRepeating(
          [](base::RepeatingClosure done_closure,
             StoragePartition* storage_partition) {
            BackgroundSyncContext* sync_context =
                storage_partition->GetBackgroundSyncContext();
            DCHECK(sync_context);
            sync_context->FireBackgroundSyncEvents(std::move(done_closure));
          },
          std::move(done_closure)));
}
#endif

BackgroundSyncLauncher::BackgroundSyncLauncher() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

BackgroundSyncLauncher::~BackgroundSyncLauncher() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

}  // namespace content