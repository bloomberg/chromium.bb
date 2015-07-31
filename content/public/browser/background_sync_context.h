// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BACKGROUND_SYNC_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_BACKGROUND_SYNC_CONTEXT_H_

#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"

namespace content {

class BackgroundSyncManager;

// One instance of this exists per StoragePartition, and services multiple
// child processes/origins. Most logic is delegated to the owned
// BackgroundSyncManager instance, which is only accessed on the IO
// thread.
class CONTENT_EXPORT BackgroundSyncContext
    : public base::RefCountedThreadSafe<BackgroundSyncContext> {
 public:
  // Only callable on the IO thread.
  virtual BackgroundSyncManager* background_sync_manager() const = 0;

 protected:
  friend class base::RefCountedThreadSafe<BackgroundSyncContext>;

  virtual ~BackgroundSyncContext() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BACKGROUND_SYNC_CONTEXT_H_
