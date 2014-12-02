// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/push_messaging/push_provider.h"

#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/threading/thread_local.h"
#include "content/child/push_messaging/push_dispatcher.h"
#include "content/child/service_worker/web_service_worker_registration_impl.h"
#include "content/child/thread_safe_sender.h"
#include "content/child/worker_task_runner.h"
#include "content/common/push_messaging_messages.h"
#include "third_party/WebKit/public/platform/WebPushError.h"
#include "third_party/WebKit/public/platform/WebPushRegistration.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace content {
namespace {

int CurrentWorkerId() {
  return WorkerTaskRunner::Instance()->CurrentWorkerId();
}

// Returns the id of the given |service_worker_registration|, which
// is only available on the implementation of the interface.
int64 GetServiceWorkerRegistrationId(
    blink::WebServiceWorkerRegistration* service_worker_registration) {
  return static_cast<WebServiceWorkerRegistrationImpl*>(
      service_worker_registration)->registration_id();
}

}  // namespace

static base::LazyInstance<base::ThreadLocalPointer<PushProvider>>::Leaky
    g_push_provider_tls = LAZY_INSTANCE_INITIALIZER;

PushProvider::PushProvider(ThreadSafeSender* thread_safe_sender,
                           PushDispatcher* push_dispatcher)
    : thread_safe_sender_(thread_safe_sender),
      push_dispatcher_(push_dispatcher) {
  g_push_provider_tls.Pointer()->Set(this);
}

PushProvider::~PushProvider() {
  STLDeleteContainerPairSecondPointers(registration_callbacks_.begin(),
                                       registration_callbacks_.end());
  registration_callbacks_.clear();
  STLDeleteContainerPairSecondPointers(permission_status_callbacks_.begin(),
                                       permission_status_callbacks_.end());
  permission_status_callbacks_.clear();
  g_push_provider_tls.Pointer()->Set(nullptr);
}

PushProvider* PushProvider::ThreadSpecificInstance(
    ThreadSafeSender* thread_safe_sender,
    PushDispatcher* push_dispatcher) {
  if (g_push_provider_tls.Pointer()->Get())
    return g_push_provider_tls.Pointer()->Get();

  PushProvider* provider =
      new PushProvider(thread_safe_sender, push_dispatcher);
  if (CurrentWorkerId())
    WorkerTaskRunner::Instance()->AddStopObserver(provider);
  return provider;
}

void PushProvider::OnWorkerRunLoopStopped() {
  delete this;
}

void PushProvider::registerPushMessaging(
    blink::WebServiceWorkerRegistration* service_worker_registration,
    blink::WebPushRegistrationCallbacks* callbacks) {
  DCHECK(service_worker_registration);
  DCHECK(callbacks);
  int request_id = push_dispatcher_->GenerateRequestId(CurrentWorkerId());
  registration_callbacks_[request_id] = callbacks;
  int64 service_worker_registration_id =
      GetServiceWorkerRegistrationId(service_worker_registration);
  thread_safe_sender_->Send(new PushMessagingHostMsg_RegisterFromWorker(
      request_id, service_worker_registration_id));
}

void PushProvider::getPermissionStatus(
    blink::WebServiceWorkerRegistration* service_worker_registration,
    blink::WebPushPermissionStatusCallbacks* callbacks) {
  DCHECK(service_worker_registration);
  DCHECK(callbacks);
  int request_id = push_dispatcher_->GenerateRequestId(CurrentWorkerId());
  permission_status_callbacks_[request_id] = callbacks;
  int64 service_worker_registration_id =
      GetServiceWorkerRegistrationId(service_worker_registration);
  thread_safe_sender_->Send(new PushMessagingHostMsg_GetPermissionStatus(
      request_id, service_worker_registration_id));
}

bool PushProvider::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PushProvider, message)
    IPC_MESSAGE_HANDLER(PushMessagingMsg_RegisterFromWorkerSuccess,
                        OnRegisterFromWorkerSuccess);
    IPC_MESSAGE_HANDLER(PushMessagingMsg_RegisterFromWorkerError,
                        OnRegisterFromWorkerError);
    IPC_MESSAGE_HANDLER(PushMessagingMsg_GetPermissionStatusSuccess,
                        OnGetPermissionStatusSuccess);
    IPC_MESSAGE_HANDLER(PushMessagingMsg_GetPermissionStatusError,
                        OnGetPermissionStatusError);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void PushProvider::OnRegisterFromWorkerSuccess(
    int request_id,
    const GURL& endpoint,
    const std::string& registration_id) {
  const auto& it = registration_callbacks_.find(request_id);
  if (it == registration_callbacks_.end())
    return;

  scoped_ptr<blink::WebPushRegistrationCallbacks> callbacks(it->second);
  registration_callbacks_.erase(it);

  scoped_ptr<blink::WebPushRegistration> registration(
      new blink::WebPushRegistration(
          blink::WebString::fromUTF8(endpoint.spec()),
          blink::WebString::fromUTF8(registration_id)));
  callbacks->onSuccess(registration.release());
}

void PushProvider::OnRegisterFromWorkerError(int request_id,
                                             PushRegistrationStatus status) {
  const auto& it = registration_callbacks_.find(request_id);
  if (it == registration_callbacks_.end())
    return;

  scoped_ptr<blink::WebPushRegistrationCallbacks> callbacks(it->second);
  registration_callbacks_.erase(it);

  scoped_ptr<blink::WebPushError> error(new blink::WebPushError(
      blink::WebPushError::ErrorTypeAbort,
      blink::WebString::fromUTF8(PushRegistrationStatusToString(status))));
  callbacks->onError(error.release());
}

void PushProvider::OnGetPermissionStatusSuccess(
    int request_id,
    blink::WebPushPermissionStatus status) {
  const auto& it = permission_status_callbacks_.find(request_id);
  if (it == permission_status_callbacks_.end())
    return;

  scoped_ptr<blink::WebPushPermissionStatusCallbacks> callbacks(it->second);
  permission_status_callbacks_.erase(it);

  callbacks->onSuccess(&status);
}

void PushProvider::OnGetPermissionStatusError(int request_id) {
  const auto& it = permission_status_callbacks_.find(request_id);
  if (it == permission_status_callbacks_.end())
    return;

  scoped_ptr<blink::WebPushPermissionStatusCallbacks> callbacks(it->second);
  permission_status_callbacks_.erase(it);

  callbacks->onError();
}

}  // namespace content
