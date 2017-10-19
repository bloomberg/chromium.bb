// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/service_worker/service_worker_dispatcher.h"

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
#include "content/child/service_worker/service_worker_handle_reference.h"
#include "content/child/service_worker/service_worker_provider_context.h"
#include "content/child/service_worker/web_service_worker_impl.h"
#include "content/child/service_worker/web_service_worker_registration_impl.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/common/content_constants.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebNavigationPreloadState.h"
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

int CurrentWorkerId() {
  return WorkerThread::GetCurrentId();
}

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
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ServiceWorkerUnregistered,
                        OnUnregistered)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_DidEnableNavigationPreload,
                        OnDidEnableNavigationPreload)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_DidGetNavigationPreloadState,
                        OnDidGetNavigationPreloadState)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_DidSetNavigationPreloadHeader,
                        OnDidSetNavigationPreloadHeader)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ServiceWorkerUnregistrationError,
                        OnUnregistrationError)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_EnableNavigationPreloadError,
                        OnEnableNavigationPreloadError)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_GetNavigationPreloadStateError,
                        OnGetNavigationPreloadStateError)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_SetNavigationPreloadHeaderError,
                        OnSetNavigationPreloadHeaderError)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ServiceWorkerStateChanged,
                        OnServiceWorkerStateChanged)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_SetVersionAttributes,
                        OnSetVersionAttributes)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_UpdateFound,
                        OnUpdateFound)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_SetControllerServiceWorker,
                        OnSetControllerServiceWorker)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_MessageToDocument,
                        OnPostMessage)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_CountFeature, OnCountFeature)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled) << "Unhandled message:" << msg.type();
}

void ServiceWorkerDispatcher::UnregisterServiceWorker(
    int provider_id,
    int64_t registration_id,
    std::unique_ptr<WebServiceWorkerUnregistrationCallbacks> callbacks) {
  DCHECK(callbacks);
  int request_id = pending_unregistration_callbacks_.Add(std::move(callbacks));
  TRACE_EVENT_ASYNC_BEGIN1("ServiceWorker",
                           "ServiceWorkerDispatcher::UnregisterServiceWorker",
                           request_id, "Registration ID", registration_id);
  thread_safe_sender_->Send(new ServiceWorkerHostMsg_UnregisterServiceWorker(
      CurrentWorkerId(), request_id, provider_id, registration_id));
}

void ServiceWorkerDispatcher::EnableNavigationPreload(
    int provider_id,
    int64_t registration_id,
    bool enable,
    std::unique_ptr<WebEnableNavigationPreloadCallbacks> callbacks) {
  DCHECK(callbacks);
  int request_id =
      enable_navigation_preload_callbacks_.Add(std::move(callbacks));
  thread_safe_sender_->Send(new ServiceWorkerHostMsg_EnableNavigationPreload(
      CurrentWorkerId(), request_id, provider_id, registration_id, enable));
}

void ServiceWorkerDispatcher::GetNavigationPreloadState(
    int provider_id,
    int64_t registration_id,
    std::unique_ptr<WebGetNavigationPreloadStateCallbacks> callbacks) {
  DCHECK(callbacks);
  int request_id =
      get_navigation_preload_state_callbacks_.Add(std::move(callbacks));
  thread_safe_sender_->Send(new ServiceWorkerHostMsg_GetNavigationPreloadState(
      CurrentWorkerId(), request_id, provider_id, registration_id));
}

void ServiceWorkerDispatcher::SetNavigationPreloadHeader(
    int provider_id,
    int64_t registration_id,
    const std::string& value,
    std::unique_ptr<WebSetNavigationPreloadHeaderCallbacks> callbacks) {
  DCHECK(callbacks);
  int request_id =
      set_navigation_preload_header_callbacks_.Add(std::move(callbacks));
  thread_safe_sender_->Send(new ServiceWorkerHostMsg_SetNavigationPreloadHeader(
      CurrentWorkerId(), request_id, provider_id, registration_id, value));
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
  // This could be possibly called multiple times to ensure termination.
  if (base::ContainsKey(provider_clients_, provider_id))
    provider_clients_.erase(provider_id);
}

ServiceWorkerDispatcher*
ServiceWorkerDispatcher::GetOrCreateThreadSpecificInstance(
    ThreadSafeSender* thread_safe_sender,
    base::SingleThreadTaskRunner* main_thread_task_runner) {
  if (g_dispatcher_tls.Pointer()->Get() == kHasBeenDeleted) {
    NOTREACHED() << "Re-instantiating TLS ServiceWorkerDispatcher.";
    g_dispatcher_tls.Pointer()->Set(NULL);
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
    return NULL;
  return static_cast<ServiceWorkerDispatcher*>(
      g_dispatcher_tls.Pointer()->Get());
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
    const ServiceWorkerVersionAttributes& attrs,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner) {
  RegistrationObjectMap::iterator found = registrations_.find(info->handle_id);
  if (found != registrations_.end()) {
    DCHECK(!info->request.is_pending());
    found->second->AttachForServiceWorkerGlobalScope(std::move(info),
                                                     std::move(io_task_runner));
    return found->second;
  }

  DCHECK(info->request.is_pending());
  // WebServiceWorkerRegistrationImpl constructor calls
  // AddServiceWorkerRegistration to add itself into |registrations_|.
  scoped_refptr<WebServiceWorkerRegistrationImpl> registration =
      WebServiceWorkerRegistrationImpl::CreateForServiceWorkerGlobalScope(
          std::move(info), std::move(io_task_runner));

  registration->SetInstalling(
      GetOrCreateServiceWorker(ServiceWorkerHandleReference::Create(
          attrs.installing, thread_safe_sender_.get())));
  registration->SetWaiting(
      GetOrCreateServiceWorker(ServiceWorkerHandleReference::Create(
          attrs.waiting, thread_safe_sender_.get())));
  registration->SetActive(
      GetOrCreateServiceWorker(ServiceWorkerHandleReference::Create(
          attrs.active, thread_safe_sender_.get())));
  return registration;
}

scoped_refptr<WebServiceWorkerRegistrationImpl>
ServiceWorkerDispatcher::GetOrCreateRegistrationForServiceWorkerClient(
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info,
    const ServiceWorkerVersionAttributes& attrs) {
  int32_t registration_handle_id = info->handle_id;

  std::unique_ptr<ServiceWorkerHandleReference> installing_ref =
      Adopt(attrs.installing);
  std::unique_ptr<ServiceWorkerHandleReference> waiting_ref =
      Adopt(attrs.waiting);
  std::unique_ptr<ServiceWorkerHandleReference> active_ref =
      Adopt(attrs.active);

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

void ServiceWorkerDispatcher::OnUnregistered(int thread_id,
                                             int request_id,
                                             bool is_success) {
  TRACE_EVENT_ASYNC_STEP_INTO0(
      "ServiceWorker",
      "ServiceWorkerDispatcher::UnregisterServiceWorker",
      request_id,
      "OnUnregistered");
  TRACE_EVENT_ASYNC_END0("ServiceWorker",
                         "ServiceWorkerDispatcher::UnregisterServiceWorker",
                         request_id);
  WebServiceWorkerUnregistrationCallbacks* callbacks =
      pending_unregistration_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  if (!callbacks)
    return;
  callbacks->OnSuccess(is_success);
  pending_unregistration_callbacks_.Remove(request_id);
}

void ServiceWorkerDispatcher::OnDidEnableNavigationPreload(int thread_id,
                                                           int request_id) {
  WebEnableNavigationPreloadCallbacks* callbacks =
      enable_navigation_preload_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  if (!callbacks)
    return;
  callbacks->OnSuccess();
  enable_navigation_preload_callbacks_.Remove(request_id);
}

void ServiceWorkerDispatcher::OnDidGetNavigationPreloadState(
    int thread_id,
    int request_id,
    const NavigationPreloadState& state) {
  WebGetNavigationPreloadStateCallbacks* callbacks =
      get_navigation_preload_state_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  if (!callbacks)
    return;
  callbacks->OnSuccess(blink::WebNavigationPreloadState(
      state.enabled, blink::WebString::FromUTF8(state.header)));
  get_navigation_preload_state_callbacks_.Remove(request_id);
}

void ServiceWorkerDispatcher::OnDidSetNavigationPreloadHeader(int thread_id,
                                                              int request_id) {
  WebSetNavigationPreloadHeaderCallbacks* callbacks =
      set_navigation_preload_header_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  if (!callbacks)
    return;
  callbacks->OnSuccess();
  set_navigation_preload_header_callbacks_.Remove(request_id);
}

void ServiceWorkerDispatcher::OnUnregistrationError(
    int thread_id,
    int request_id,
    blink::mojom::ServiceWorkerErrorType error_type,
    const base::string16& message) {
  TRACE_EVENT_ASYNC_STEP_INTO0(
      "ServiceWorker",
      "ServiceWorkerDispatcher::UnregisterServiceWorker",
      request_id,
      "OnUnregistrationError");
  TRACE_EVENT_ASYNC_END0("ServiceWorker",
                         "ServiceWorkerDispatcher::UnregisterServiceWorker",
                         request_id);
  WebServiceWorkerUnregistrationCallbacks* callbacks =
      pending_unregistration_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  if (!callbacks)
    return;

  callbacks->OnError(
      WebServiceWorkerError(error_type, blink::WebString::FromUTF16(message)));
  pending_unregistration_callbacks_.Remove(request_id);
}

void ServiceWorkerDispatcher::OnEnableNavigationPreloadError(
    int thread_id,
    int request_id,
    blink::mojom::ServiceWorkerErrorType error_type,
    const std::string& message) {
  WebEnableNavigationPreloadCallbacks* callbacks =
      enable_navigation_preload_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  if (!callbacks)
    return;
  callbacks->OnError(
      WebServiceWorkerError(error_type, blink::WebString::FromUTF8(message)));
  enable_navigation_preload_callbacks_.Remove(request_id);
}

void ServiceWorkerDispatcher::OnGetNavigationPreloadStateError(
    int thread_id,
    int request_id,
    blink::mojom::ServiceWorkerErrorType error_type,
    const std::string& message) {
  WebGetNavigationPreloadStateCallbacks* callbacks =
      get_navigation_preload_state_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  if (!callbacks)
    return;
  callbacks->OnError(
      WebServiceWorkerError(error_type, blink::WebString::FromUTF8(message)));
  get_navigation_preload_state_callbacks_.Remove(request_id);
}

void ServiceWorkerDispatcher::OnSetNavigationPreloadHeaderError(
    int thread_id,
    int request_id,
    blink::mojom::ServiceWorkerErrorType error_type,
    const std::string& message) {
  WebSetNavigationPreloadHeaderCallbacks* callbacks =
      set_navigation_preload_header_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  if (!callbacks)
    return;
  callbacks->OnError(
      WebServiceWorkerError(error_type, blink::WebString::FromUTF8(message)));
  set_navigation_preload_header_callbacks_.Remove(request_id);
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

void ServiceWorkerDispatcher::OnSetVersionAttributes(
    int thread_id,
    int registration_handle_id,
    int changed_mask,
    const ServiceWorkerVersionAttributes& attrs) {
  TRACE_EVENT1("ServiceWorker",
               "ServiceWorkerDispatcher::OnSetVersionAttributes",
               "Thread ID", thread_id);

  // Adopt the references sent from the browser process and pass it to the
  // registration if it exists.
  std::unique_ptr<ServiceWorkerHandleReference> installing =
      Adopt(attrs.installing);
  std::unique_ptr<ServiceWorkerHandleReference> waiting = Adopt(attrs.waiting);
  std::unique_ptr<ServiceWorkerHandleReference> active = Adopt(attrs.active);

  RegistrationObjectMap::iterator found =
      registrations_.find(registration_handle_id);
  if (found != registrations_.end()) {
    // Populate the version fields (eg. .installing) with worker objects.
    ChangedVersionAttributesMask mask(changed_mask);
    if (mask.installing_changed())
      found->second->SetInstalling(
          GetOrCreateServiceWorker(std::move(installing)));
    if (mask.waiting_changed())
      found->second->SetWaiting(GetOrCreateServiceWorker(std::move(waiting)));
    if (mask.active_changed())
      found->second->SetActive(GetOrCreateServiceWorker(std::move(active)));
  }
}

void ServiceWorkerDispatcher::OnUpdateFound(
    int thread_id,
    int registration_handle_id) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerDispatcher::OnUpdateFound");
  RegistrationObjectMap::iterator found =
      registrations_.find(registration_handle_id);
  if (found != registrations_.end())
    found->second->OnUpdateFound();
}

void ServiceWorkerDispatcher::OnSetControllerServiceWorker(
    const ServiceWorkerMsg_SetControllerServiceWorker_Params& params) {
  TRACE_EVENT2(
      "ServiceWorker", "ServiceWorkerDispatcher::OnSetControllerServiceWorker",
      "Thread ID", params.thread_id, "Provider ID", params.provider_id);

  // Adopt the reference sent from the browser process and pass it to the
  // provider context if it exists.
  std::unique_ptr<ServiceWorkerHandleReference> handle_ref =
      Adopt(params.object_info);
  ProviderContextMap::iterator provider =
      provider_contexts_.find(params.provider_id);
  if (provider != provider_contexts_.end()) {
    provider->second->SetController(std::move(handle_ref),
                                    params.used_features);
  }

  ProviderClientMap::iterator found =
      provider_clients_.find(params.provider_id);
  if (found != provider_clients_.end()) {
    // Sync the controllee's use counter with the service worker's one.
    for (uint32_t feature : params.used_features)
      found->second->CountFeature(feature);

    // Get the existing worker object or create a new one with a new reference
    // to populate the .controller field.
    scoped_refptr<WebServiceWorkerImpl> worker =
        GetOrCreateServiceWorker(ServiceWorkerHandleReference::Create(
            params.object_info, thread_safe_sender_.get()));
    found->second->SetController(WebServiceWorkerImpl::CreateHandle(worker),
                                 params.should_notify_controllerchange);
    // You must not access |found| after setController() because it may fire the
    // controllerchange event that may remove the provider client, for example,
    // by detaching an iframe.
  }
}

void ServiceWorkerDispatcher::OnPostMessage(
    const ServiceWorkerMsg_MessageToDocument_Params& params) {
  // Make sure we're on the main document thread. (That must be the only
  // thread we get this message)
  DCHECK_EQ(kDocumentMainThreadId, params.thread_id);
  TRACE_EVENT1("ServiceWorker", "ServiceWorkerDispatcher::OnPostMessage",
               "Thread ID", params.thread_id);

  // Adopt the reference sent from the browser process and get the corresponding
  // worker object.
  scoped_refptr<WebServiceWorkerImpl> worker =
      GetOrCreateServiceWorker(Adopt(params.service_worker_info));

  ProviderClientMap::iterator found =
      provider_clients_.find(params.provider_id);
  if (found == provider_clients_.end()) {
    // For now we do no queueing for messages sent to nonexistent / unattached
    // client.
    return;
  }

  found->second->DispatchMessageEvent(
      WebServiceWorkerImpl::CreateHandle(worker),
      blink::WebString::FromUTF16(params.message),
      std::move(params.message_ports));
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

std::unique_ptr<ServiceWorkerHandleReference> ServiceWorkerDispatcher::Adopt(
    const blink::mojom::ServiceWorkerObjectInfo& info) {
  return ServiceWorkerHandleReference::Adopt(info, thread_safe_sender_.get());
}

}  // namespace content
