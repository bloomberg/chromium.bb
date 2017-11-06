// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_dispatcher.h"

#include <stddef.h>
#include <utility>

#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/common/content_constants.h"
#include "content/renderer/service_worker/service_worker_handle_reference.h"
#include "content/renderer/service_worker/service_worker_provider_context.h"
#include "content/renderer/service_worker/web_service_worker_impl.h"
#include "content/renderer/service_worker/web_service_worker_registration_impl.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerProviderClient.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_error_type.mojom.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_registration.mojom.h"
#include "url/url_constants.h"

using blink::WebServiceWorkerError;
using blink::WebServiceWorkerProvider;
using base::ThreadLocalPointer;

namespace content {

namespace {

base::LazyInstance<ThreadLocalPointer<void>>::Leaky g_dispatcher_tls =
    LAZY_INSTANCE_INITIALIZER;

void* const kHasBeenDeleted = reinterpret_cast<void*>(0x1);

}  // namespace

ServiceWorkerDispatcher::ServiceWorkerDispatcher(
    ThreadSafeSender* thread_safe_sender,
    base::SingleThreadTaskRunner* main_thread_task_runner)
    : thread_safe_sender_(thread_safe_sender),
      main_thread_task_runner_(main_thread_task_runner) {
  g_dispatcher_tls.Pointer()->Set(static_cast<void*>(this));
}

ServiceWorkerDispatcher::~ServiceWorkerDispatcher() {
  g_dispatcher_tls.Pointer()->Set(kHasBeenDeleted);
}

void ServiceWorkerDispatcher::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;

  // When you add a new message handler, you should consider adding a similar
  // handler in ServiceWorkerMessageFilter to release references passed from
  // the browser process in case we fail to post task to the thread.
  IPC_BEGIN_MESSAGE_MAP(ServiceWorkerDispatcher, msg)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ServiceWorkerStateChanged,
                        OnServiceWorkerStateChanged)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_CountFeature, OnCountFeature)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled) << "Unhandled message:" << msg.type();
}

void ServiceWorkerDispatcher::AddProviderContext(
    ServiceWorkerProviderContext* provider_context) {
  DCHECK(provider_context);
  int provider_id = provider_context->provider_id();
  DCHECK(!base::ContainsKey(provider_contexts_, provider_id));
  provider_contexts_[provider_id] = provider_context;
}

void ServiceWorkerDispatcher::RemoveProviderContext(
    ServiceWorkerProviderContext* provider_context) {
  DCHECK(provider_context);
  DCHECK(
      base::ContainsKey(provider_contexts_, provider_context->provider_id()));
  provider_contexts_.erase(provider_context->provider_id());
}

void ServiceWorkerDispatcher::AddProviderClient(
    int provider_id,
    blink::WebServiceWorkerProviderClient* client) {
  DCHECK(client);
  DCHECK(!base::ContainsKey(provider_clients_, provider_id));
  provider_clients_[provider_id] = client;
}

void ServiceWorkerDispatcher::RemoveProviderClient(int provider_id) {
  auto iter = provider_clients_.find(provider_id);
  // This could be possibly called multiple times to ensure termination.
  if (iter != provider_clients_.end())
    provider_clients_.erase(iter);
}

blink::WebServiceWorkerProviderClient*
ServiceWorkerDispatcher::GetProviderClient(int provider_id) {
  auto iter = provider_clients_.find(provider_id);
  if (iter != provider_clients_.end())
    return iter->second;
  return nullptr;
}

ServiceWorkerDispatcher*
ServiceWorkerDispatcher::GetOrCreateThreadSpecificInstance(
    ThreadSafeSender* thread_safe_sender,
    base::SingleThreadTaskRunner* main_thread_task_runner) {
  if (g_dispatcher_tls.Pointer()->Get() == kHasBeenDeleted) {
    NOTREACHED() << "Re-instantiating TLS ServiceWorkerDispatcher.";
    g_dispatcher_tls.Pointer()->Set(nullptr);
  }
  if (g_dispatcher_tls.Pointer()->Get())
    return static_cast<ServiceWorkerDispatcher*>(
        g_dispatcher_tls.Pointer()->Get());

  ServiceWorkerDispatcher* dispatcher =
      new ServiceWorkerDispatcher(thread_safe_sender, main_thread_task_runner);
  if (WorkerThread::GetCurrentId())
    WorkerThread::AddObserver(dispatcher);
  return dispatcher;
}

ServiceWorkerDispatcher* ServiceWorkerDispatcher::GetThreadSpecificInstance() {
  if (g_dispatcher_tls.Pointer()->Get() == kHasBeenDeleted)
    return nullptr;
  return static_cast<ServiceWorkerDispatcher*>(
      g_dispatcher_tls.Pointer()->Get());
}

std::unique_ptr<ServiceWorkerHandleReference> ServiceWorkerDispatcher::Adopt(
    blink::mojom::ServiceWorkerObjectInfoPtr info) {
  return ServiceWorkerHandleReference::Adopt(std::move(info),
                                             thread_safe_sender_);
}

void ServiceWorkerDispatcher::WillStopCurrentWorkerThread() {
  delete this;
}

scoped_refptr<WebServiceWorkerImpl>
ServiceWorkerDispatcher::GetOrCreateServiceWorker(
    std::unique_ptr<ServiceWorkerHandleReference> handle_ref) {
  if (!handle_ref)
    return nullptr;

  WorkerObjectMap::iterator found =
      service_workers_.find(handle_ref->handle_id());
  if (found != service_workers_.end())
    return found->second;

  // WebServiceWorkerImpl constructor calls AddServiceWorker.
  return new WebServiceWorkerImpl(std::move(handle_ref),
                                  thread_safe_sender_.get());
}

scoped_refptr<WebServiceWorkerRegistrationImpl>
ServiceWorkerDispatcher::GetOrCreateRegistrationForServiceWorkerGlobalScope(
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner) {
  RegistrationObjectMap::iterator found = registrations_.find(info->handle_id);
  if (found != registrations_.end()) {
    DCHECK(!info->request.is_pending());
    found->second->AttachForServiceWorkerGlobalScope(std::move(info),
                                                     std::move(io_task_runner));
    return found->second;
  }

  std::unique_ptr<ServiceWorkerHandleReference> installing_ref =
      ServiceWorkerHandleReference::Create(std::move(info->installing),
                                           thread_safe_sender_);
  std::unique_ptr<ServiceWorkerHandleReference> waiting_ref =
      ServiceWorkerHandleReference::Create(std::move(info->waiting),
                                           thread_safe_sender_);
  std::unique_ptr<ServiceWorkerHandleReference> active_ref =
      ServiceWorkerHandleReference::Create(std::move(info->active),
                                           thread_safe_sender_);
  DCHECK(info->request.is_pending());
  // WebServiceWorkerRegistrationImpl constructor calls
  // AddServiceWorkerRegistration to add itself into |registrations_|.
  scoped_refptr<WebServiceWorkerRegistrationImpl> registration =
      WebServiceWorkerRegistrationImpl::CreateForServiceWorkerGlobalScope(
          std::move(info), std::move(io_task_runner));

  registration->SetInstalling(
      GetOrCreateServiceWorker(std::move(installing_ref)));
  registration->SetWaiting(GetOrCreateServiceWorker(std::move(waiting_ref)));
  registration->SetActive(GetOrCreateServiceWorker(std::move(active_ref)));

  return registration;
}

scoped_refptr<WebServiceWorkerRegistrationImpl>
ServiceWorkerDispatcher::GetOrCreateRegistrationForServiceWorkerClient(
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info) {
  int32_t registration_handle_id = info->handle_id;

  std::unique_ptr<ServiceWorkerHandleReference> installing_ref =
      Adopt(std::move(info->installing));
  std::unique_ptr<ServiceWorkerHandleReference> waiting_ref =
      Adopt(std::move(info->waiting));
  std::unique_ptr<ServiceWorkerHandleReference> active_ref =
      Adopt(std::move(info->active));

  RegistrationObjectMap::iterator found =
      registrations_.find(registration_handle_id);
  if (found != registrations_.end()) {
    DCHECK(!info->request.is_pending());
    found->second->AttachForServiceWorkerClient(std::move(info));
    return found->second;
  }

  DCHECK(info->request.is_pending());
  // WebServiceWorkerRegistrationImpl constructor calls
  // AddServiceWorkerRegistration to add itself into |registrations_|.
  scoped_refptr<WebServiceWorkerRegistrationImpl> registration =
      WebServiceWorkerRegistrationImpl::CreateForServiceWorkerClient(
          std::move(info));

  registration->SetInstalling(
      GetOrCreateServiceWorker(std::move(installing_ref)));
  registration->SetWaiting(GetOrCreateServiceWorker(std::move(waiting_ref)));
  registration->SetActive(GetOrCreateServiceWorker(std::move(active_ref)));
  return registration;
}

void ServiceWorkerDispatcher::OnServiceWorkerStateChanged(
    int thread_id,
    int handle_id,
    blink::mojom::ServiceWorkerState state) {
  TRACE_EVENT2("ServiceWorker",
               "ServiceWorkerDispatcher::OnServiceWorkerStateChanged",
               "Thread ID", thread_id,
               "State", static_cast<int>(state));
  WorkerObjectMap::iterator worker = service_workers_.find(handle_id);
  if (worker != service_workers_.end())
    worker->second->OnStateChanged(state);
}

void ServiceWorkerDispatcher::OnCountFeature(int thread_id,
                                             int provider_id,
                                             uint32_t feature) {
  ProviderContextMap::iterator provider = provider_contexts_.find(provider_id);
  if (provider != provider_contexts_.end()) {
    provider->second->CountFeature(feature);
  }

  ProviderClientMap::iterator found = provider_clients_.find(provider_id);
  if (found != provider_clients_.end())
    found->second->CountFeature(feature);
}

void ServiceWorkerDispatcher::AddServiceWorker(
    int handle_id, WebServiceWorkerImpl* worker) {
  DCHECK(!base::ContainsKey(service_workers_, handle_id));
  service_workers_[handle_id] = worker;
}

void ServiceWorkerDispatcher::RemoveServiceWorker(int handle_id) {
  DCHECK(base::ContainsKey(service_workers_, handle_id));
  service_workers_.erase(handle_id);
}

void ServiceWorkerDispatcher::AddServiceWorkerRegistration(
    int registration_handle_id,
    WebServiceWorkerRegistrationImpl* registration) {
  DCHECK(!base::ContainsKey(registrations_, registration_handle_id));
  registrations_[registration_handle_id] = registration;
}

void ServiceWorkerDispatcher::RemoveServiceWorkerRegistration(
    int registration_handle_id) {
  DCHECK(base::ContainsKey(registrations_, registration_handle_id));
  registrations_.erase(registration_handle_id);
}

}  // namespace content
