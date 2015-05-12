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

// CallbackThreadAdapter<S,T> is a wrapper for WebCallbacks<S,T> which
// switches to a specific thread before calling the wrapped callback's
// onSuccess or onError methods.
//
// Takes ownership of the WebCallbacks object which it wraps.
template <typename S, typename T>
class CallbackThreadAdapter : public blink::WebCallbacks<S, T> {
 public:
  CallbackThreadAdapter(scoped_ptr<blink::WebCallbacks<S, T>> callbacks,
                        int worker_thread_id)
      : worker_thread_id_(worker_thread_id) {
    callbacks_.reset(callbacks.release());
  }

  virtual void onSuccess(S* results) {
    // If the worker thread has been destroyed, then this task will be
    // silently discarded.
    WorkerTaskRunner::Instance()->PostTask(
        worker_thread_id_,
        base::Bind(&blink::WebCallbacks<S, T>::onSuccess,
                   base::Unretained(callbacks_.get()), results));
  }

  virtual void onError(T* error) {
    // If the worker thread has been destroyed, then this task will be
    // silently discarded.
    WorkerTaskRunner::Instance()->PostTask(
        worker_thread_id_,
        base::Bind(&blink::WebCallbacks<S, T>::onError,
                   base::Unretained(callbacks_.get()), error));
  }

 private:
  scoped_ptr<blink::WebCallbacks<S, T>> callbacks_;
  int worker_thread_id_;
};

LazyInstance<ThreadLocalPointer<BackgroundSyncProviderThreadProxy>>::Leaky
    g_sync_provider_tls = LAZY_INSTANCE_INITIALIZER;

}  // anonymous namespace

BackgroundSyncProviderThreadProxy*
BackgroundSyncProviderThreadProxy::GetThreadInstance(
    base::SingleThreadTaskRunner* main_thread_task_runner,
    BackgroundSyncProvider* sync_provider) {
  if (g_sync_provider_tls.Pointer()->Get())
    return g_sync_provider_tls.Pointer()->Get();

  BackgroundSyncProviderThreadProxy* instance =
      new BackgroundSyncProviderThreadProxy(main_thread_task_runner,
                                            sync_provider);
  DCHECK(WorkerTaskRunner::Instance()->CurrentWorkerId());
  WorkerTaskRunner::Instance()->AddStopObserver(instance);
  return instance;
}

void BackgroundSyncProviderThreadProxy::registerBackgroundSync(
    const blink::WebSyncRegistration* options,
    blink::WebServiceWorkerRegistration* service_worker_registration,
    blink::WebSyncRegistrationCallbacks* callbacks) {
  DCHECK(options);
  DCHECK(service_worker_registration);
  DCHECK(callbacks);
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&BackgroundSyncProvider::registerBackgroundSync,
                 base::Unretained(sync_provider_), options,
                 service_worker_registration,
                 new CallbackThreadAdapter<blink::WebSyncRegistration,
                                           blink::WebSyncError>(
                     make_scoped_ptr(callbacks),
                     WorkerTaskRunner::Instance()->CurrentWorkerId())));
}

void BackgroundSyncProviderThreadProxy::unregisterBackgroundSync(
    blink::WebSyncRegistration::Periodicity periodicity,
    int64_t id,
    const blink::WebString& tag,
    blink::WebServiceWorkerRegistration* service_worker_registration,
    blink::WebSyncUnregistrationCallbacks* callbacks) {
  DCHECK(service_worker_registration);
  DCHECK(callbacks);
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&BackgroundSyncProvider::unregisterBackgroundSync,
                 base::Unretained(sync_provider_), periodicity, id, tag,
                 service_worker_registration,
                 new CallbackThreadAdapter<bool, blink::WebSyncError>(
                     make_scoped_ptr(callbacks),
                     WorkerTaskRunner::Instance()->CurrentWorkerId())));
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
      base::Bind(&BackgroundSyncProvider::getRegistration,
                 base::Unretained(sync_provider_), periodicity, tag,
                 service_worker_registration,
                 new CallbackThreadAdapter<blink::WebSyncRegistration,
                                           blink::WebSyncError>(
                     make_scoped_ptr(callbacks),
                     WorkerTaskRunner::Instance()->CurrentWorkerId())));
}

void BackgroundSyncProviderThreadProxy::getRegistrations(
    blink::WebSyncRegistration::Periodicity periodicity,
    blink::WebServiceWorkerRegistration* service_worker_registration,
    blink::WebSyncGetRegistrationsCallbacks* callbacks) {
  DCHECK(service_worker_registration);
  DCHECK(callbacks);
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&BackgroundSyncProvider::getRegistrations,
                 base::Unretained(sync_provider_), periodicity,
                 service_worker_registration,
                 new CallbackThreadAdapter<
                     blink::WebVector<blink::WebSyncRegistration*>,
                     blink::WebSyncError>(
                     make_scoped_ptr(callbacks),
                     WorkerTaskRunner::Instance()->CurrentWorkerId())));
}

void BackgroundSyncProviderThreadProxy::OnWorkerRunLoopStopped() {
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
