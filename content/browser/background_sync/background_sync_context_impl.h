// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_CONTEXT_IMPL_H_
#define CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_CONTEXT_IMPL_H_

#include <set>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/background_sync_service.mojom.h"
#include "content/common/content_export.h"
#include "content/public/browser/background_sync_context.h"

namespace content {

class BackgroundSyncManager;
class BackgroundSyncServiceImpl;
class ServiceWorkerContextWrapper;

// Implements the BackgroundSyncContext. One instance of this exists per
// StoragePartition, and services multiple child processes/origins. Most logic
// is delegated to the owned BackgroundSyncManager instance, which is only
// accessed on the IO thread.
class CONTENT_EXPORT BackgroundSyncContextImpl : public BackgroundSyncContext {
 public:
  BackgroundSyncContextImpl();

  // Init and Shutdown are for use on the UI thread when the
  // StoragePartition is being setup and torn down.
  void Init(const scoped_refptr<ServiceWorkerContextWrapper>& context);

  // Shutdown must be called before deleting this. Call on the UI thread.
  void Shutdown();

  // Create a BackgroundSyncServiceImpl that is owned by this. Call on the UI
  // thread.
  void CreateService(mojo::InterfaceRequest<BackgroundSyncService> request);

  // Called by BackgroundSyncServiceImpl objects so that they can
  // be deleted. Call on the IO thread.
  void ServiceHadConnectionError(BackgroundSyncServiceImpl* service);

  // Call on the IO thread.
  BackgroundSyncManager* background_sync_manager() const override;

 protected:
  ~BackgroundSyncContextImpl() override;

 private:
  friend class BackgroundSyncServiceImplTest;

  void CreateBackgroundSyncManager(
      const scoped_refptr<ServiceWorkerContextWrapper>& context);

  void CreateServiceOnIOThread(
      mojo::InterfaceRequest<BackgroundSyncService> request);

  void ShutdownOnIO();

  // Only accessed on the IO thread.
  scoped_ptr<BackgroundSyncManager> background_sync_manager_;

  // The services are owned by this. They're either deleted
  // during ShutdownOnIO or when the channel is closed via
  // ServiceHadConnectionError. Only accessed on the IO thread.
  std::set<BackgroundSyncServiceImpl*> services_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundSyncContextImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_CONTEXT_IMPL_H_
