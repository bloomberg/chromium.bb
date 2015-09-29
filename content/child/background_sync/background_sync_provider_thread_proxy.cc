// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/background_sync/background_sync_provider_thread_proxy.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_local.h"
#include "content/child/background_sync/background_sync_provider.h"

using base::LazyInstance;
using base::ThreadLocalPointer;

namespace content {

namespace {

template <typename T>
struct WebCallbacksMatcher;

template <typename Stype, typename Ttype>
struct WebCallbacksMatcher<blink::WebCallbacks<Stype, Ttype>> {
  using S = Stype;
  using T = Ttype;
  using WebCallbacks = typename blink::WebCallbacks<S, T>;
};

// CallbackThreadAdapter<WebCallbacks<S, T>> is a wrapper for WebCallbacks<S, T>
// which switches to a specific thread before calling the wrapped callback's
// onSuccess or onError methods.
//
// Takes ownership of the WebCallbacks object which it wraps.
template <typename X>
class CallbackThreadAdapter : public WebCallbacksMatcher<X>::WebCallbacks {
  using S = typename WebCallbacksMatcher<X>::S;
  using T = typename WebCallbacksMatcher<X>::T;
  using OnSuccessType = void (blink::WebCallbacks<S, T>::*)(S);
  using OnErrorType = void (blink::WebCallbacks<S, T>::*)(T);

 public:
  CallbackThreadAdapter(scoped_ptr<blink::WebCallbacks<S, T>> callbacks,
                        int worker_thread_id)
      : worker_thread_id_(worker_thread_id) {
    callbacks_.reset(callbacks.release());
  }

  virtual void onSuccess(S results) {
    OnSuccessType on_success = &blink::WebCallbacks<S, T>::onSuccess;
    // If the worker thread has been destroyed, then this task will be
    // silently discarded.
    WorkerTaskRunner::Instance()->PostTask(
        worker_thread_id_,
        base::Bind(on_success, base::Owned(callbacks_.release()), results));
  }

  virtual void onError(T error) {
    OnErrorType on_error = &blink::WebCallbacks<S, T>::onError;
    // If the worker thread has been destroyed, then this task will be
    // silently discarded.
    WorkerTaskRunner::Instance()->PostTask(
        worker_thread_id_,
        base::Bind(on_error, base::Owned(callbacks_.release()), error));
  }

 private:
  scoped_ptr<blink::WebCallbacks<S, T>> callbacks_;
  int worker_thread_id_;
};

LazyInstance<ThreadLocalPointer<BackgroundSyncProviderThreadProxy>>::Leaky
    g_sync_provider_tls = LAZY_INSTANCE_INITIALIZER;

void DuplicateRegistrationHandleCallbackOnSWThread(
    const BackgroundSyncProviderThreadProxy::
        DuplicateRegistrationHandleCallback& callback,
    BackgroundSyncError error,
    SyncRegistrationPtr registration) {
  callback.Run(error, registration.Pass());
}

void DuplicateRegistrationHandleCallbackOnMainThread(
    int worker_thread_id,
    const BackgroundSyncProviderThreadProxy::
        DuplicateRegistrationHandleCallback& callback,
    BackgroundSyncError error,
    SyncRegistrationPtr registration) {
  WorkerTaskRunner::Instance()->PostTask(
      worker_thread_id,
      base::Bind(&DuplicateRegistrationHandleCallbackOnSWThread, callback,
                 error, base::Passed(registration.Pass())));
}

}  // anonymous namespace

// static
BackgroundSyncProviderThreadProxy*
BackgroundSyncProviderThreadProxy::GetThreadInstance(
    base::SingleThreadTaskRunner* main_thread_task_runner,
    BackgroundSyncProvider* sync_provider) {
  if (g_sync_provider_tls.Pointer()->Get())
    return g_sync_provider_tls.Pointer()->Get();

  if (!WorkerThread::GetCurrentId()) {
    // This could happen if GetThreadInstance is called very late (say by a
    // garbage collected SyncRegistration).
    return nullptr;
  }

  BackgroundSyncProviderThreadProxy* instance =
      new BackgroundSyncProviderThreadProxy(main_thread_task_runner,
                                            sync_provider);
  WorkerThread::AddObserver(instance);
  return instance;
}

void BackgroundSyncProviderThreadProxy::registerBackgroundSync(
    const blink::WebSyncRegistration* options,
    blink::WebServiceWorkerRegistration* service_worker_registration,
    bool requested_from_service_worker,
    blink::WebSyncRegistrationCallbacks* callbacks) {
  DCHECK(options);
  DCHECK(service_worker_registration);
  DCHECK(callbacks);
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &BackgroundSyncProvider::registerBackgroundSync,
          base::Unretained(sync_provider_), options,
          service_worker_registration, requested_from_service_worker,
          new CallbackThreadAdapter<blink::WebSyncRegistrationCallbacks>(
              make_scoped_ptr(callbacks), WorkerThread::GetCurrentId())));
}

void BackgroundSyncProviderThreadProxy::unregisterBackgroundSync(
    blink::WebSyncRegistration::Periodicity periodicity,
    int64_t handle_id,
    const blink::WebString& tag,
    blink::WebServiceWorkerRegistration* service_worker_registration,
    blink::WebSyncUnregistrationCallbacks* callbacks) {
  DCHECK(service_worker_registration);
  DCHECK(callbacks);
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &BackgroundSyncProvider::unregisterBackgroundSync,
          base::Unretained(sync_provider_), periodicity, handle_id,
          // We cast WebString to string16 before crossing threads
          // for thread-safety.
          static_cast<base::string16>(tag), service_worker_registration,
          new CallbackThreadAdapter<blink::WebSyncUnregistrationCallbacks>(
              make_scoped_ptr(callbacks), WorkerThread::GetCurrentId())));
}

void BackgroundSyncProviderThreadProxy::getRegistration(
    blink::WebSyncRegistration::Periodicity periodicity,
    const blink::WebString& tag,
    blink::WebServiceWorkerRegistration* service_worker_registration,
    blink::WebSyncRegistrationCallbacks* callbacks) {
  DCHECK(service_worker_registration);
  DCHECK(callbacks);
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &BackgroundSyncProvider::getRegistration,
          base::Unretained(sync_provider_), periodicity,
          // We cast WebString to string16 before crossing threads
          // for thread-safety.
          static_cast<base::string16>(tag), service_worker_registration,
          new CallbackThreadAdapter<blink::WebSyncRegistrationCallbacks>(
              make_scoped_ptr(callbacks), WorkerThread::GetCurrentId())));
}

void BackgroundSyncProviderThreadProxy::getRegistrations(
    blink::WebSyncRegistration::Periodicity periodicity,
    blink::WebServiceWorkerRegistration* service_worker_registration,
    blink::WebSyncGetRegistrationsCallbacks* callbacks) {
  DCHECK(service_worker_registration);
  DCHECK(callbacks);
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &BackgroundSyncProvider::getRegistrations,
          base::Unretained(sync_provider_), periodicity,
          service_worker_registration,
          new CallbackThreadAdapter<blink::WebSyncGetRegistrationsCallbacks>(
              make_scoped_ptr(callbacks), WorkerThread::GetCurrentId())));
}

void BackgroundSyncProviderThreadProxy::getPermissionStatus(
    blink::WebSyncRegistration::Periodicity periodicity,
    blink::WebServiceWorkerRegistration* service_worker_registration,
    blink::WebSyncGetPermissionStatusCallbacks* callbacks) {
  DCHECK(service_worker_registration);
  DCHECK(callbacks);
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &BackgroundSyncProvider::getPermissionStatus,
          base::Unretained(sync_provider_), periodicity,
          service_worker_registration,
          new CallbackThreadAdapter<blink::WebSyncGetPermissionStatusCallbacks>(
              make_scoped_ptr(callbacks), WorkerThread::GetCurrentId())));
}

void BackgroundSyncProviderThreadProxy::releaseRegistration(int64_t handle_id) {
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&BackgroundSyncProvider::releaseRegistration,
                            base::Unretained(sync_provider_), handle_id));
}

void BackgroundSyncProviderThreadProxy::notifyWhenDone(
    int64_t handle_id,
    blink::WebSyncNotifyWhenDoneCallbacks* callbacks) {
  DCHECK(callbacks);

  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &BackgroundSyncProvider::notifyWhenDone,
          base::Unretained(sync_provider_), handle_id,
          new CallbackThreadAdapter<blink::WebSyncNotifyWhenDoneCallbacks>(
              make_scoped_ptr(callbacks), WorkerThread::GetCurrentId())));
}

void BackgroundSyncProviderThreadProxy::DuplicateRegistrationHandle(
    int64_t handle_id,
    const DuplicateRegistrationHandleCallback& callback) {
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&BackgroundSyncProvider::DuplicateRegistrationHandle,
                 base::Unretained(sync_provider_), handle_id,
                 base::Bind(&DuplicateRegistrationHandleCallbackOnMainThread,
                            WorkerThread::GetCurrentId(), callback)));
}

void BackgroundSyncProviderThreadProxy::WillStopCurrentWorkerThread() {
  delete this;
}

BackgroundSyncProviderThreadProxy::BackgroundSyncProviderThreadProxy(
    base::SingleThreadTaskRunner* main_thread_task_runner,
    BackgroundSyncProvider* sync_provider)
    : main_thread_task_runner_(main_thread_task_runner),
      sync_provider_(sync_provider) {
  g_sync_provider_tls.Pointer()->Set(this);
}

BackgroundSyncProviderThreadProxy::~BackgroundSyncProviderThreadProxy() {
  g_sync_provider_tls.Pointer()->Set(nullptr);
}

}  // namespace content
