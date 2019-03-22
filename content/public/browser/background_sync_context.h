// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BACKGROUND_SYNC_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_BACKGROUND_SYNC_CONTEXT_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "build/build_config.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#endif

namespace content {

class BrowserContext;
class StoragePartition;

// One instance of this exists per StoragePartition, and services multiple child
// processes/origins. It contains the context for processing Background Sync
// registrations, and delegates most of this processing to owned instances of
// other components.
class CONTENT_EXPORT BackgroundSyncContext {
 public:
  // Gets the soonest time delta from now, when the browser should be woken up
  // to fire any Background Sync events, across all storage partitions in
  // |browser_context|, and invokes |callback| with it.
  static void GetSoonestWakeupDeltaAcrossPartitions(
      BrowserContext* browser_context,
      base::OnceCallback<void(base::TimeDelta)> callback);

#if defined(OS_ANDROID)
  // Processes pending Background Sync registrations for all storage partitions
  // in |browser_context|, and then runs  the |j_runnable| when done.
  static void FireBackgroundSyncEventsAcrossPartitions(
      BrowserContext* browser_context,
      const base::android::JavaParamRef<jobject>& j_runnable);
#endif

  BackgroundSyncContext() = default;

  // Process any pending Background Sync registrations.
  // This involves firing any sync events ready to be fired, and optionally
  // scheduling a job to wake up the browser when the next event needs to be
  // fired.
  virtual void FireBackgroundSyncEvents(base::OnceClosure done_closure) = 0;

  // Gets the soonest time delta from now, when the browser should be woken up
  // to fire any Background Sync events. Calls |callback| with this value.
  virtual void GetSoonestWakeupDelta(
      base::OnceCallback<void(base::TimeDelta)> callback) = 0;

 protected:
  virtual ~BackgroundSyncContext() = default;

  DISALLOW_COPY_AND_ASSIGN(BackgroundSyncContext);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BACKGROUND_SYNC_CONTEXT_H_
