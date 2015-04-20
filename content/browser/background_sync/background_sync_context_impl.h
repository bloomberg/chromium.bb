// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_CONTEXT_IMPL_H_
#define CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_CONTEXT_IMPL_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"

namespace content {

class BackgroundSyncManager;
class ServiceWorkerContextWrapper;

// One instance of this exists per StoragePartition, and services multiple
// child processes/origins. Most logic is delegated to the owned
// BackgroundSyncManager instance, which is only accessed on the IO
// thread.
// TODO(jkarlin): Make a public/ BackgroundSyncContext.
class CONTENT_EXPORT BackgroundSyncContextImpl
    : public base::RefCountedThreadSafe<BackgroundSyncContextImpl> {
 public:
  BackgroundSyncContextImpl();

  // Init and Shutdown are for use on the UI thread when the profile,
  // storagepartition is being setup and torn down.
  void Init(const scoped_refptr<ServiceWorkerContextWrapper>& context);
  void Shutdown();

  // Only callable on the IO thread.
  BackgroundSyncManager* background_sync_manager() const;

 private:
  friend class base::RefCountedThreadSafe<BackgroundSyncContextImpl>;

  ~BackgroundSyncContextImpl();

  void CreateBackgroundSyncManager(
      const scoped_refptr<ServiceWorkerContextWrapper>& context);

  void ShutdownOnIO();

  // Only accessed on the IO thread.
  scoped_ptr<BackgroundSyncManager> background_sync_manager_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundSyncContextImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_CONTEXT_IMPL_H_
