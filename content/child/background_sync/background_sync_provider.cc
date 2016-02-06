// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/background_sync/background_sync_provider.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_local.h"
#include "content/child/background_sync/background_sync_type_converters.h"
#include "content/child/child_thread_impl.h"
#include "content/child/service_worker/web_service_worker_registration_impl.h"
#include "content/public/common/background_sync.mojom.h"
#include "third_party/WebKit/public/platform/modules/background_sync/WebSyncError.h"
#include "third_party/WebKit/public/platform/modules/background_sync/WebSyncRegistration.h"

using base::LazyInstance;
using base::ThreadLocalPointer;

namespace content {
namespace {

// Returns the id of the given |service_worker_registration|, which
// is only available on the implementation of the interface.
int64_t GetServiceWorkerRegistrationId(
    blink::WebServiceWorkerRegistration* service_worker_registration) {
  return static_cast<WebServiceWorkerRegistrationImpl*>(
             service_worker_registration)->registration_id();
}

void ConnectToServiceOnMainThread(
    mojo::InterfaceRequest<BackgroundSyncService> request) {
  DCHECK(ChildThreadImpl::current());
  ChildThreadImpl::current()->service_registry()->ConnectToRemoteService(
      std::move(request));
}

LazyInstance<ThreadLocalPointer<BackgroundSyncProvider>>::Leaky
    g_sync_provider_tls = LAZY_INSTANCE_INITIALIZER;

}  // namespace

BackgroundSyncProvider::BackgroundSyncProvider(
    const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner)
    : main_thread_task_runner_(main_task_runner) {
  DCHECK(main_task_runner);
  g_sync_provider_tls.Pointer()->Set(this);
}

BackgroundSyncProvider::~BackgroundSyncProvider() {
  g_sync_provider_tls.Pointer()->Set(nullptr);
}

// static
BackgroundSyncProvider*
BackgroundSyncProvider::GetOrCreateThreadSpecificInstance(
    base::SingleThreadTaskRunner* main_thread_task_runner) {
  DCHECK(main_thread_task_runner);
  if (g_sync_provider_tls.Pointer()->Get())
    return g_sync_provider_tls.Pointer()->Get();

  bool have_worker_id = (WorkerThread::GetCurrentId() > 0);
  if (!main_thread_task_runner->BelongsToCurrentThread() && !have_worker_id) {
    // On a worker thread, this could happen if this method is called
    // very late (say by a garbage collected SyncRegistration).
    return nullptr;
  }

  BackgroundSyncProvider* instance =
      new BackgroundSyncProvider(main_thread_task_runner);

  if (have_worker_id) {
    // For worker threads, use the observer interface to clean up when workers
    // are stopped.
    WorkerThread::AddObserver(instance);
  }

  return instance;
}

void BackgroundSyncProvider::registerBackgroundSync(
    const blink::WebSyncRegistration* options,
    blink::WebServiceWorkerRegistration* service_worker_registration,
    bool requested_from_service_worker,
    blink::WebSyncRegistrationCallbacks* callbacks) {
  DCHECK(options);
  DCHECK(service_worker_registration);
  DCHECK(callbacks);
  int64_t service_worker_registration_id =
      GetServiceWorkerRegistrationId(service_worker_registration);
  scoped_ptr<const blink::WebSyncRegistration> optionsPtr(options);
  scoped_ptr<blink::WebSyncRegistrationCallbacks> callbacksPtr(callbacks);

  // base::Unretained is safe here, as the mojo channel will be deleted (and
  // will wipe its callbacks) before 'this' is deleted.
  GetBackgroundSyncServicePtr()->Register(
      mojo::ConvertTo<SyncRegistrationPtr>(*(optionsPtr.get())),
      service_worker_registration_id, requested_from_service_worker,
      base::Bind(&BackgroundSyncProvider::RegisterCallback,
                 base::Unretained(this),
                 base::Passed(std::move(callbacksPtr))));
}

void BackgroundSyncProvider::unregisterBackgroundSync(
    int64_t handle_id,
    blink::WebServiceWorkerRegistration* service_worker_registration,
    blink::WebSyncUnregistrationCallbacks* callbacks) {
  DCHECK(service_worker_registration);
  DCHECK(callbacks);
  int64_t service_worker_registration_id =
      GetServiceWorkerRegistrationId(service_worker_registration);
  scoped_ptr<blink::WebSyncUnregistrationCallbacks> callbacksPtr(callbacks);

  // base::Unretained is safe here, as the mojo channel will be deleted (and
  // will wipe its callbacks) before 'this' is deleted.
  GetBackgroundSyncServicePtr()->Unregister(
      handle_id, service_worker_registration_id,
      base::Bind(&BackgroundSyncProvider::UnregisterCallback,
                 base::Unretained(this),
                 base::Passed(std::move(callbacksPtr))));
}

void BackgroundSyncProvider::getRegistration(
    const blink::WebString& tag,
    blink::WebServiceWorkerRegistration* service_worker_registration,
    blink::WebSyncRegistrationCallbacks* callbacks) {
  DCHECK(service_worker_registration);
  DCHECK(callbacks);
  int64_t service_worker_registration_id =
      GetServiceWorkerRegistrationId(service_worker_registration);
  scoped_ptr<blink::WebSyncRegistrationCallbacks> callbacksPtr(callbacks);

  // base::Unretained is safe here, as the mojo channel will be deleted (and
  // will wipe its callbacks) before 'this' is deleted.
  GetBackgroundSyncServicePtr()->GetRegistration(
      tag.utf8(), service_worker_registration_id,
      base::Bind(&BackgroundSyncProvider::GetRegistrationCallback,
                 base::Unretained(this),
                 base::Passed(std::move(callbacksPtr))));
}

void BackgroundSyncProvider::getRegistrations(
    blink::WebServiceWorkerRegistration* service_worker_registration,
    blink::WebSyncGetRegistrationsCallbacks* callbacks) {
  DCHECK(service_worker_registration);
  DCHECK(callbacks);
  int64_t service_worker_registration_id =
      GetServiceWorkerRegistrationId(service_worker_registration);
  scoped_ptr<blink::WebSyncGetRegistrationsCallbacks> callbacksPtr(callbacks);

  // base::Unretained is safe here, as the mojo channel will be deleted (and
  // will wipe its callbacks) before 'this' is deleted.
  GetBackgroundSyncServicePtr()->GetRegistrations(
      service_worker_registration_id,
      base::Bind(&BackgroundSyncProvider::GetRegistrationsCallback,
                 base::Unretained(this),
                 base::Passed(std::move(callbacksPtr))));
}

void BackgroundSyncProvider::releaseRegistration(int64_t handle_id) {
  GetBackgroundSyncServicePtr()->ReleaseRegistration(handle_id);
}

void BackgroundSyncProvider::DuplicateRegistrationHandle(
    int64_t handle_id,
    const BackgroundSyncService::DuplicateRegistrationHandleCallback&
        callback) {
  GetBackgroundSyncServicePtr()->DuplicateRegistrationHandle(handle_id,
                                                             callback);
}

void BackgroundSyncProvider::WillStopCurrentWorkerThread() {
  delete this;
}

void BackgroundSyncProvider::RegisterCallback(
    scoped_ptr<blink::WebSyncRegistrationCallbacks> callbacks,
    BackgroundSyncError error,
    const SyncRegistrationPtr& options) {
  // TODO(iclelland): Determine the correct error message to return in each case
  scoped_ptr<blink::WebSyncRegistration> result;
  switch (error) {
    case BackgroundSyncError::NONE:
      if (!options.is_null())
        result =
            mojo::ConvertTo<scoped_ptr<blink::WebSyncRegistration>>(options);
      callbacks->onSuccess(blink::adoptWebPtr(result.release()));
      break;
    case BackgroundSyncError::NOT_FOUND:
      NOTREACHED();
      break;
    case BackgroundSyncError::STORAGE:
      callbacks->onError(
          blink::WebSyncError(blink::WebSyncError::ErrorTypeUnknown,
                              "Background Sync is disabled."));
      break;
    case BackgroundSyncError::NOT_ALLOWED:
      callbacks->onError(
          blink::WebSyncError(blink::WebSyncError::ErrorTypeNoPermission,
                              "Attempted to register a sync event without a "
                              "window or registration tag too long."));
      break;
    case BackgroundSyncError::NO_SERVICE_WORKER:
      callbacks->onError(
          blink::WebSyncError(blink::WebSyncError::ErrorTypeUnknown,
                              "No service worker is active."));
      break;
  }
}

void BackgroundSyncProvider::UnregisterCallback(
    scoped_ptr<blink::WebSyncUnregistrationCallbacks> callbacks,
    BackgroundSyncError error) {
  // TODO(iclelland): Determine the correct error message to return in each case
  switch (error) {
    case BackgroundSyncError::NONE:
      callbacks->onSuccess(true);
      break;
    case BackgroundSyncError::NOT_FOUND:
      callbacks->onSuccess(false);
      break;
    case BackgroundSyncError::STORAGE:
      callbacks->onError(
          blink::WebSyncError(blink::WebSyncError::ErrorTypeUnknown,
                              "Background Sync is disabled."));
      break;
    case BackgroundSyncError::NOT_ALLOWED:
      // This error should never be returned from
      // BackgroundSyncManager::Unregister
      NOTREACHED();
      break;
    case BackgroundSyncError::NO_SERVICE_WORKER:
      callbacks->onError(
          blink::WebSyncError(blink::WebSyncError::ErrorTypeUnknown,
                              "No service worker is active."));
      break;
  }
}

void BackgroundSyncProvider::GetRegistrationCallback(
    scoped_ptr<blink::WebSyncRegistrationCallbacks> callbacks,
    BackgroundSyncError error,
    const SyncRegistrationPtr& options) {
  // TODO(iclelland): Determine the correct error message to return in each case
  scoped_ptr<blink::WebSyncRegistration> result;
  switch (error) {
    case BackgroundSyncError::NONE:
      if (!options.is_null())
        result =
            mojo::ConvertTo<scoped_ptr<blink::WebSyncRegistration>>(options);
      callbacks->onSuccess(blink::adoptWebPtr(result.release()));
      break;
    case BackgroundSyncError::NOT_FOUND:
      callbacks->onSuccess(nullptr);
      break;
    case BackgroundSyncError::STORAGE:
      callbacks->onError(
          blink::WebSyncError(blink::WebSyncError::ErrorTypeUnknown,
                              "Background Sync is disabled."));
      break;
    case BackgroundSyncError::NOT_ALLOWED:
      // This error should never be returned from
      // BackgroundSyncManager::GetRegistration
      NOTREACHED();
      break;
    case BackgroundSyncError::NO_SERVICE_WORKER:
      callbacks->onError(
          blink::WebSyncError(blink::WebSyncError::ErrorTypeUnknown,
                              "No service worker is active."));
      break;
  }
}

void BackgroundSyncProvider::GetRegistrationsCallback(
    scoped_ptr<blink::WebSyncGetRegistrationsCallbacks> callbacks,
    BackgroundSyncError error,
    const mojo::Array<SyncRegistrationPtr>& registrations) {
  // TODO(iclelland): Determine the correct error message to return in each case
  switch (error) {
    case BackgroundSyncError::NONE: {
      blink::WebVector<blink::WebSyncRegistration*> results(
          registrations.size());
      for (size_t i = 0; i < registrations.size(); ++i) {
        results[i] = mojo::ConvertTo<scoped_ptr<blink::WebSyncRegistration>>(
                         registrations[i])
                         .release();
      }
      callbacks->onSuccess(results);
      break;
    }
    case BackgroundSyncError::NOT_FOUND:
    case BackgroundSyncError::NOT_ALLOWED:
      // These errors should never be returned from
      // BackgroundSyncManager::GetRegistrations
      NOTREACHED();
      break;
    case BackgroundSyncError::STORAGE:
      callbacks->onError(
          blink::WebSyncError(blink::WebSyncError::ErrorTypeUnknown,
                              "Background Sync is disabled."));
      break;
    case BackgroundSyncError::NO_SERVICE_WORKER:
      callbacks->onError(
          blink::WebSyncError(blink::WebSyncError::ErrorTypeUnknown,
                              "No service worker is active."));
      break;
  }
}

BackgroundSyncServicePtr&
BackgroundSyncProvider::GetBackgroundSyncServicePtr() {
  if (!background_sync_service_.get()) {
    mojo::InterfaceRequest<BackgroundSyncService> request =
        mojo::GetProxy(&background_sync_service_);
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&ConnectToServiceOnMainThread, base::Passed(&request)));
  }
  return background_sync_service_;
}

}  // namespace content
