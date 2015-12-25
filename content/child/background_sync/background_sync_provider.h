// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_BACKGROUND_SYNC_BACKGROUND_SYNC_PROVIDER_H_
#define CONTENT_CHILD_BACKGROUND_SYNC_BACKGROUND_SYNC_PROVIDER_H_

#include <stdint.h>

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/background_sync_service.mojom.h"
#include "content/public/child/worker_thread.h"
#include "third_party/WebKit/public/platform/modules/background_sync/WebSyncProvider.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace content {

// The BackgroundSyncProvider is called by the SyncManager and SyncRegistration
// objects (and their Periodic counterparts) and communicates with the
// BackgroundSyncManager object in the browser process. Each thread will have
// its own instance (e.g. main thread, worker threads), instantiated as needed
// by BlinkPlatformImpl.  Each instance of the provider creates a new mojo
// connection to a new BackgroundSyncManagerImpl, which then talks to the
// BackgroundSyncManager object.
class BackgroundSyncProvider : public blink::WebSyncProvider,
                               public WorkerThread::Observer {
 public:
  // Constructor made public to allow BlinkPlatformImpl to own a copy for the
  // main thread. Everyone else should use GetOrCreateThreadSpecificInstance().
  explicit BackgroundSyncProvider(
      const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner);

  ~BackgroundSyncProvider() override;

  // Returns thread-specific instance (if exists).  Otherwise, a new instance
  // will be created for the thread, except when called for a worker thread that
  // has already been stopped.
  static BackgroundSyncProvider* GetOrCreateThreadSpecificInstance(
      base::SingleThreadTaskRunner* main_thread_task_runner);

  // blink::WebSyncProvider implementation
  void registerBackgroundSync(
      const blink::WebSyncRegistration* options,
      blink::WebServiceWorkerRegistration* service_worker_registration,
      bool requested_from_service_worker,
      blink::WebSyncRegistrationCallbacks* callbacks) override;
  void unregisterBackgroundSync(
      int64_t handle_id,
      blink::WebServiceWorkerRegistration* service_worker_registration,
      blink::WebSyncUnregistrationCallbacks* callbacks) override;
  void getRegistration(
      blink::WebSyncRegistration::Periodicity,
      const blink::WebString& tag,
      blink::WebServiceWorkerRegistration* service_worker_registration,
      blink::WebSyncRegistrationCallbacks* callbacks) override;
  void getRegistrations(
      blink::WebSyncRegistration::Periodicity periodicity,
      blink::WebServiceWorkerRegistration* service_worker_registration,
      blink::WebSyncGetRegistrationsCallbacks* callbacks) override;
  void getPermissionStatus(
      blink::WebSyncRegistration::Periodicity periodicity,
      blink::WebServiceWorkerRegistration* service_worker_registration,
      blink::WebSyncGetPermissionStatusCallbacks* callbacks) override;
  // TODO(jkarlin): Rename to releaseRegistrationHandle.
  void releaseRegistration(int64_t handle_id) override;
  void notifyWhenFinished(
      int64_t handle_id,
      blink::WebSyncNotifyWhenFinishedCallbacks* callbacks) override;

  void DuplicateRegistrationHandle(
      int64_t handle_id,
      const BackgroundSyncService::DuplicateRegistrationHandleCallback&
          callback);

  // WorkerThread::Observer implementation.
  void WillStopCurrentWorkerThread() override;

 private:
  // Callback handlers
  void RegisterCallback(
      scoped_ptr<blink::WebSyncRegistrationCallbacks> callbacks,
      BackgroundSyncError error,
      const SyncRegistrationPtr& options);
  void UnregisterCallback(
      scoped_ptr<blink::WebSyncUnregistrationCallbacks> callbacks,
      BackgroundSyncError error);
  void GetRegistrationCallback(
      scoped_ptr<blink::WebSyncRegistrationCallbacks> callbacks,
      BackgroundSyncError error,
      const SyncRegistrationPtr& options);
  void GetRegistrationsCallback(
      scoped_ptr<blink::WebSyncGetRegistrationsCallbacks> callbacks,
      BackgroundSyncError error,
      const mojo::Array<SyncRegistrationPtr>& registrations);
  void GetPermissionStatusCallback(
      scoped_ptr<blink::WebSyncGetPermissionStatusCallbacks> callbacks,
      BackgroundSyncError error,
      PermissionStatus status);
  void NotifyWhenFinishedCallback(
      scoped_ptr<blink::WebSyncNotifyWhenFinishedCallbacks> callbacks,
      BackgroundSyncError error,
      BackgroundSyncState state);

  // Helper method that returns an initialized BackgroundSyncServicePtr.
  BackgroundSyncServicePtr& GetBackgroundSyncServicePtr();

  BackgroundSyncServicePtr background_sync_service_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundSyncProvider);
};

}  // namespace content

#endif  // CONTENT_CHILD_BACKGROUND_SYNC_BACKGROUND_SYNC_PROVIDER_H_
