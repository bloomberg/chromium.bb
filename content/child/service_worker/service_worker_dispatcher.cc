// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/service_worker/service_worker_dispatcher.h"

#include "base/lazy_instance.h"
#include "base/threading/thread_local.h"
#include "content/child/service_worker/web_service_worker_impl.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"

using blink::WebServiceWorkerError;
using blink::WebServiceWorkerProvider;
using base::ThreadLocalPointer;

namespace content {

namespace {

base::LazyInstance<ThreadLocalPointer<ServiceWorkerDispatcher> >::Leaky
    g_dispatcher_tls = LAZY_INSTANCE_INITIALIZER;

ServiceWorkerDispatcher* const kHasBeenDeleted =
    reinterpret_cast<ServiceWorkerDispatcher*>(0x1);

int CurrentWorkerId() {
  return WorkerTaskRunner::Instance()->CurrentWorkerId();
}

}  // namespace

ServiceWorkerDispatcher::ServiceWorkerDispatcher(
    ThreadSafeSender* thread_safe_sender)
    : thread_safe_sender_(thread_safe_sender) {
  g_dispatcher_tls.Pointer()->Set(this);
}

ServiceWorkerDispatcher::~ServiceWorkerDispatcher() {
  g_dispatcher_tls.Pointer()->Set(kHasBeenDeleted);
}

void ServiceWorkerDispatcher::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ServiceWorkerDispatcher, msg)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ServiceWorkerRegistered, OnRegistered)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ServiceWorkerUnregistered,
                        OnUnregistered)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ServiceWorkerRegistrationError,
                        OnRegistrationError)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled) << "Unhandled message:" << msg.type();
}

bool ServiceWorkerDispatcher::Send(IPC::Message* msg) {
  return thread_safe_sender_->Send(msg);
}

void ServiceWorkerDispatcher::RegisterServiceWorker(
    const GURL& pattern,
    const GURL& script_url,
    WebServiceWorkerProvider::WebServiceWorkerCallbacks* callbacks) {
  DCHECK(callbacks);
  int request_id = pending_callbacks_.Add(callbacks);
  thread_safe_sender_->Send(new ServiceWorkerHostMsg_RegisterServiceWorker(
      CurrentWorkerId(), request_id, pattern, script_url));
}

void ServiceWorkerDispatcher::UnregisterServiceWorker(
    const GURL& pattern,
    WebServiceWorkerProvider::WebServiceWorkerCallbacks* callbacks) {
  DCHECK(callbacks);
  int request_id = pending_callbacks_.Add(callbacks);
  thread_safe_sender_->Send(new ServiceWorkerHostMsg_UnregisterServiceWorker(
      CurrentWorkerId(), request_id, pattern));
}

void ServiceWorkerDispatcher::AddScriptClient(
    int provider_id,
    blink::WebServiceWorkerProviderClient* client) {
  DCHECK(client);
  DCHECK(!ContainsKey(script_clients_, provider_id));
  script_clients_[provider_id] = client;
  thread_safe_sender_->Send(new ServiceWorkerHostMsg_AddScriptClient(
      CurrentWorkerId(), provider_id));
}

void ServiceWorkerDispatcher::RemoveScriptClient(int provider_id) {
  // This could be possibly called multiple times to ensure termination.
  if (ContainsKey(script_clients_, provider_id)) {
    script_clients_.erase(provider_id);
    thread_safe_sender_->Send(new ServiceWorkerHostMsg_RemoveScriptClient(
        CurrentWorkerId(), provider_id));
  }
}

ServiceWorkerDispatcher* ServiceWorkerDispatcher::ThreadSpecificInstance(
    ThreadSafeSender* thread_safe_sender) {
  if (g_dispatcher_tls.Pointer()->Get() == kHasBeenDeleted) {
    NOTREACHED() << "Re-instantiating TLS ServiceWorkerDispatcher.";
    g_dispatcher_tls.Pointer()->Set(NULL);
  }
  if (g_dispatcher_tls.Pointer()->Get())
    return g_dispatcher_tls.Pointer()->Get();

  ServiceWorkerDispatcher* dispatcher =
      new ServiceWorkerDispatcher(thread_safe_sender);
  if (WorkerTaskRunner::Instance()->CurrentWorkerId())
    WorkerTaskRunner::Instance()->AddStopObserver(dispatcher);
  return dispatcher;
}

void ServiceWorkerDispatcher::OnRegistered(int32 thread_id,
                                           int32 request_id,
                                           int handle_id) {
  WebServiceWorkerProvider::WebServiceWorkerCallbacks* callbacks =
      pending_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  if (!callbacks)
    return;

  // the browser has to generate the registration_id so the same
  // worker can be called from different renderer contexts. However,
  // the impl object doesn't have to be the same instance across calls
  // unless we require the DOM objects to be identical when there's a
  // duplicate registration. So for now we mint a new object each
  // time.
  scoped_ptr<WebServiceWorkerImpl> worker(
      new WebServiceWorkerImpl(handle_id, thread_safe_sender_));
  callbacks->onSuccess(worker.release());
  pending_callbacks_.Remove(request_id);
}

void ServiceWorkerDispatcher::OnUnregistered(
    int32 thread_id,
    int32 request_id) {
  WebServiceWorkerProvider::WebServiceWorkerCallbacks* callbacks =
      pending_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  if (!callbacks)
    return;

  callbacks->onSuccess(NULL);
  pending_callbacks_.Remove(request_id);
}

void ServiceWorkerDispatcher::OnRegistrationError(
    int32 thread_id,
    int32 request_id,
    WebServiceWorkerError::ErrorType error_type,
    const base::string16& message) {
  WebServiceWorkerProvider::WebServiceWorkerCallbacks* callbacks =
      pending_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  if (!callbacks)
    return;

  scoped_ptr<WebServiceWorkerError>  error(
      new WebServiceWorkerError(error_type, message));
  callbacks->onError(error.release());
  pending_callbacks_.Remove(request_id);
}

void ServiceWorkerDispatcher::OnWorkerRunLoopStopped() { delete this; }

}  // namespace content
