// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_BACKGROUND_SYNC_BACKGROUND_SYNC_PROVIDER_H_
#define CONTENT_CHILD_BACKGROUND_SYNC_BACKGROUND_SYNC_PROVIDER_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "content/child/worker_task_runner.h"
#include "content/common/background_sync_service.mojom.h"
#include "third_party/WebKit/public/platform/modules/background_sync/WebSyncProvider.h"

namespace content {

class ServiceRegistry;

// The BackgroundSyncProvider is called by the SyncManager and SyncRegistration
// objects (and their Periodic counterparts) and communicates with the
// BackgroundSyncManager object in the browser process. This class is
// instantiated on the main thread by BlinkPlatformImpl, and its methods can be
// called directly from the main thread.
class BackgroundSyncProvider : public blink::WebSyncProvider {
 public:
  explicit BackgroundSyncProvider(ServiceRegistry* service_registry);

  ~BackgroundSyncProvider() override;

  // blink::WebSyncProvider implementation
  void registerBackgroundSync(
      const blink::WebSyncRegistration* options,
      blink::WebServiceWorkerRegistration* service_worker_registration,
      blink::WebSyncRegistrationCallbacks* callbacks);
  void unregisterBackgroundSync(
      blink::WebSyncRegistration::Periodicity periodicity,
      int64_t id,
      const blink::WebString& tag,
      blink::WebServiceWorkerRegistration* service_worker_registration,
      blink::WebSyncUnregistrationCallbacks* callbacks);
  void getRegistration(
      blink::WebSyncRegistration::Periodicity,
      const blink::WebString& tag,
      blink::WebServiceWorkerRegistration* service_worker_registration,
      blink::WebSyncRegistrationCallbacks* callbacks);
  void getRegistrations(
      blink::WebSyncRegistration::Periodicity periodicity,
      blink::WebServiceWorkerRegistration* service_worker_registration,
      blink::WebSyncGetRegistrationsCallbacks* callbacks);

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

  // Helper method that returns an initialized BackgroundSyncServicePtr.
  BackgroundSyncServicePtr& GetBackgroundSyncServicePtr();

  ServiceRegistry* service_registry_;
  BackgroundSyncServicePtr background_sync_service_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundSyncProvider);
};

}  // namespace content

#endif  // CONTENT_CHILD_BACKGROUND_SYNC_BACKGROUND_SYNC_PROVIDER_H_
