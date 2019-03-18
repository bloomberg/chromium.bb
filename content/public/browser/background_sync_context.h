// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BACKGROUND_SYNC_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_BACKGROUND_SYNC_CONTEXT_H_

#include "base/callback_forward.h"
#include "base/macros.h"

namespace content {

class StoragePartition;

// One instance of this exists per StoragePartition, and services multiple child
// processes/origins. It contains the context for processing Background Sync
// registrations, and delegates most of this processing to owned instances of
// other components.
class CONTENT_EXPORT BackgroundSyncContext {
 public:
  BackgroundSyncContext() = default;

  // Process any pending Background Sync registrations for |storage_partition|.
  // This involves firing any sync events ready to be fired, and optionally
  // scheduling a job to wake up the browser when the next event needs to be
  // fired.
  virtual void FireBackgroundSyncEventsForStoragePartition(
      StoragePartition* storage_partition,
      base::OnceClosure done_closure) = 0;

 protected:
  virtual ~BackgroundSyncContext() = default;

  DISALLOW_COPY_AND_ASSIGN(BackgroundSyncContext);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BACKGROUND_SYNC_CONTEXT_H_
