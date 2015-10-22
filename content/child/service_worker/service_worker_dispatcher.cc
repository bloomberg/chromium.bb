// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/service_worker/service_worker_dispatcher.h"

#include "base/lazy_instance.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread_local.h"
#include "base/trace_event/trace_event.h"
#include "content/child/child_thread_impl.h"
#include "content/child/service_worker/service_worker_handle_reference.h"
#include "content/child/service_worker/service_worker_provider_context.h"
#include "content/child/service_worker/service_worker_registration_handle_reference.h"
#include "content/child/service_worker/web_service_worker_impl.h"
#include "content/child/service_worker/web_service_worker_registration_impl.h"
#include "content/child/thread_safe_sender.h"
#include "content/child/webmessageportchannel_impl.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/common/url_utils.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerClientsInfo.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerProviderClient.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"

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
  IPC_BEGIN_MESSAGE_MAP(ServiceWorkerDispatcher, msg)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_AssociateRegistrationWithServiceWorker,
                        OnAssociateRegistrationWithServiceWorker)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_AssociateRegistration,
                        OnAssociateRegistration)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_DisassociateRegistration,
                        OnDisassociateRegistration)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ServiceWorkerRegistered, OnRegistered)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ServiceWorkerUpdated, OnUpdated)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ServiceWorkerUnregistered,
                        OnUnregistered)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_DidGetRegistration,
                        OnDidGetRegistration)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_DidGetRegistrations,
                        OnDidGetRegistrations)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_DidGetRegistrationForReady,
                        OnDidGetRegistrationForReady)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ServiceWorkerRegistrationError,
                        OnRegistrationError)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ServiceWorkerUpdateError,
                        OnUpdateError)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ServiceWorkerUnregistrationError,
                        OnUnregistrationError)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ServiceWorkerGetRegistrationError,
                        OnGetRegistrationError)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ServiceWorkerGetRegistrationsError,
                        OnGetRegistrationsError)
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
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled) << "Unhandled message:" << msg.type();
}

bool ServiceWorkerDispatcher::Send(IPC::Message* msg) {
  return thread_safe_sender_->Send(msg);
}

void ServiceWorkerDispatcher::RegisterServiceWorker(
    int provider_id,
    const GURL& pattern,
    const GURL& script_url,
    WebServiceWorkerRegistrationCallbacks* callbacks) {
  DCHECK(callbacks);

  if (pattern.possibly_invalid_spec().size() > GetMaxURLChars() ||
      script_url.possibly_invalid_spec().size() > GetMaxURLChars()) {
    scoped_ptr<WebServiceWorkerRegistrationCallbacks>
        owned_callbacks(callbacks);
    std::string error_message(kServiceWorkerRegisterErrorPrefix);
    error_message += "The provided scriptURL or scope is too long.";
    callbacks->onError(
        WebServiceWorkerError(WebServiceWorkerError::ErrorTypeSecurity,
                              blink::WebString::fromUTF8(error_message)));
    return;
  }

  int request_id = pending_registration_callbacks_.Add(callbacks);
  TRACE_EVENT_ASYNC_BEGIN2("ServiceWorker",
                           "ServiceWorkerDispatcher::RegisterServiceWorker",
                           request_id,
                           "Scope", pattern.spec(),
                           "Script URL", script_url.spec());
  thread_safe_sender_->Send(new ServiceWorkerHostMsg_RegisterServiceWorker(
      CurrentWorkerId(), request_id, provider_id, pattern, script_url));
}

void ServiceWorkerDispatcher::UpdateServiceWorker(
    int provider_id,
    int64 registration_id,
    WebServiceWorkerUpdateCallbacks* callbacks) {
  DCHECK(callbacks);
  int request_id = pending_update_callbacks_.Add(callbacks);
  thread_safe_sender_->Send(new ServiceWorkerHostMsg_UpdateServiceWorker(
      CurrentWorkerId(), request_id, provider_id, registration_id));
}

void ServiceWorkerDispatcher::UnregisterServiceWorker(
    int provider_id,
    int64 registration_id,
    WebServiceWorkerUnregistrationCallbacks* callbacks) {
  DCHECK(callbacks);
  int request_id = pending_unregistration_callbacks_.Add(callbacks);
  TRACE_EVENT_ASYNC_BEGIN1("ServiceWorker",
                           "ServiceWorkerDispatcher::UnregisterServiceWorker",
                           request_id, "Registration ID", registration_id);
  thread_safe_sender_->Send(new ServiceWorkerHostMsg_UnregisterServiceWorker(
      CurrentWorkerId(), request_id, provider_id, registration_id));
}

void ServiceWorkerDispatcher::GetRegistration(
    int provider_id,
    const GURL& document_url,
    WebServiceWorkerGetRegistrationCallbacks* callbacks) {
  DCHECK(callbacks);

  if (document_url.possibly_invalid_spec().size() > GetMaxURLChars()) {
    scoped_ptr<WebServiceWorkerGetRegistrationCallbacks> owned_callbacks(
        callbacks);
    std::string error_message(kServiceWorkerGetRegistrationErrorPrefix);
    error_message += "The provided documentURL is too long.";
    callbacks->onError(
        WebServiceWorkerError(WebServiceWorkerError::ErrorTypeSecurity,
                              blink::WebString::fromUTF8(error_message)));
    return;
  }

  int request_id = pending_get_registration_callbacks_.Add(callbacks);
  TRACE_EVENT_ASYNC_BEGIN1("ServiceWorker",
                           "ServiceWorkerDispatcher::GetRegistration",
                           request_id,
                           "Document URL", document_url.spec());
  thread_safe_sender_->Send(new ServiceWorkerHostMsg_GetRegistration(
      CurrentWorkerId(), request_id, provider_id, document_url));
}

void ServiceWorkerDispatcher::GetRegistrations(
    int provider_id,
    WebServiceWorkerGetRegistrationsCallbacks* callbacks) {
  DCHECK(callbacks);

  int request_id = pending_get_registrations_callbacks_.Add(callbacks);
  TRACE_EVENT_ASYNC_BEGIN0("ServiceWorker",
                           "ServiceWorkerDispatcher::GetRegistrations",
                           request_id);
  thread_safe_sender_->Send(new ServiceWorkerHostMsg_GetRegistrations(
      CurrentWorkerId(), request_id, provider_id));
}

void ServiceWorkerDispatcher::GetRegistrationForReady(
    int provider_id,
    WebServiceWorkerGetRegistrationForReadyCallbacks* callbacks) {
  int request_id = get_for_ready_callbacks_.Add(callbacks);
  TRACE_EVENT_ASYNC_BEGIN0("ServiceWorker",
                           "ServiceWorkerDispatcher::GetRegistrationForReady",
                           request_id);
  thread_safe_sender_->Send(new ServiceWorkerHostMsg_GetRegistrationForReady(
      CurrentWorkerId(), request_id, provider_id));
}

void ServiceWorkerDispatcher::AddProviderContext(
    ServiceWorkerProviderContext* provider_context) {
  DCHECK(provider_context);
  int provider_id = provider_context->provider_id();
  DCHECK(!ContainsKey(provider_contexts_, provider_id));
  provider_contexts_[provider_id] = provider_context;
}

void ServiceWorkerDispatcher::RemoveProviderContext(
    ServiceWorkerProviderContext* provider_context) {
  DCHECK(provider_context);
  DCHECK(ContainsKey(provider_contexts_, provider_context->provider_id()));
  provider_contexts_.erase(provider_context->provider_id());
}

void ServiceWorkerDispatcher::AddProviderClient(
    int provider_id,
    blink::WebServiceWorkerProviderClient* client) {
  DCHECK(client);
  DCHECK(!ContainsKey(provider_clients_, provider_id));
  provider_clients_[provider_id] = client;
}

void ServiceWorkerDispatcher::RemoveProviderClient(int provider_id) {
  // This could be possibly called multiple times to ensure termination.
  if (ContainsKey(provider_clients_, provider_id))
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
    const ServiceWorkerObjectInfo& info) {
  if (info.handle_id == kInvalidServiceWorkerHandleId)
    return nullptr;

  WorkerObjectMap::iterator found = service_workers_.find(info.handle_id);
  if (found != service_workers_.end())
    return found->second;

  // WebServiceWorkerImpl constructor calls AddServiceWorker.
  return new WebServiceWorkerImpl(
      ServiceWorkerHandleReference::Create(info, thread_safe_sender_.get()),
      thread_safe_sender_.get());
}

scoped_refptr<WebServiceWorkerImpl>
ServiceWorkerDispatcher::GetOrAdoptServiceWorker(
    const ServiceWorkerObjectInfo& info) {
  if (info.handle_id == kInvalidServiceWorkerHandleId)
    return nullptr;

  scoped_ptr<ServiceWorkerHandleReference> handle_ref =
      ServiceWorkerHandleReference::Adopt(info, thread_safe_sender_.get());

  WorkerObjectMap::iterator found = service_workers_.find(info.handle_id);
  if (found != service_workers_.end())
    return found->second;

  // WebServiceWorkerImpl constructor calls AddServiceWorker.
  return new WebServiceWorkerImpl(handle_ref.Pass(), thread_safe_sender_.get());
}

scoped_refptr<WebServiceWorkerRegistrationImpl>
ServiceWorkerDispatcher::GetOrCreateRegistration(
    const ServiceWorkerRegistrationObjectInfo& info,
    const ServiceWorkerVersionAttributes& attrs) {
  RegistrationObjectMap::iterator found = registrations_.find(info.handle_id);
  if (found != registrations_.end())
    return found->second;

  // WebServiceWorkerRegistrationImpl constructor calls
  // AddServiceWorkerRegistration.
  scoped_refptr<WebServiceWorkerRegistrationImpl> registration(
      new WebServiceWorkerRegistrationImpl(
          ServiceWorkerRegistrationHandleReference::Create(
              info, thread_safe_sender_.get())));

  registration->SetInstalling(GetOrCreateServiceWorker(attrs.installing));
  registration->SetWaiting(GetOrCreateServiceWorker(attrs.waiting));
  registration->SetActive(GetOrCreateServiceWorker(attrs.active));
  return registration.Pass();
}

scoped_refptr<WebServiceWorkerRegistrationImpl>
ServiceWorkerDispatcher::GetOrAdoptRegistration(
    const ServiceWorkerRegistrationObjectInfo& info,
    const ServiceWorkerVersionAttributes& attrs) {
  RegistrationObjectMap::iterator found = registrations_.find(info.handle_id);
  if (found != registrations_.end()) {
    ServiceWorkerHandleReference::Adopt(attrs.installing,
                                        thread_safe_sender_.get());
    ServiceWorkerHandleReference::Adopt(attrs.waiting,
                                        thread_safe_sender_.get());
    ServiceWorkerHandleReference::Adopt(attrs.active,
                                        thread_safe_sender_.get());
    ServiceWorkerRegistrationHandleReference::Adopt(info,
                                                    thread_safe_sender_.get());
    return found->second;
  }

  // WebServiceWorkerRegistrationImpl constructor calls
  // AddServiceWorkerRegistration.
  scoped_refptr<WebServiceWorkerRegistrationImpl> registration(
      new WebServiceWorkerRegistrationImpl(
          ServiceWorkerRegistrationHandleReference::Adopt(
              info, thread_safe_sender_.get())));

  registration->SetInstalling(GetOrAdoptServiceWorker(attrs.installing));
  registration->SetWaiting(GetOrAdoptServiceWorker(attrs.waiting));
  registration->SetActive(GetOrAdoptServiceWorker(attrs.active));
  return registration.Pass();
}

// We can assume that this message handler is called before the worker context
// starts because script loading happens after this association.
void ServiceWorkerDispatcher::OnAssociateRegistrationWithServiceWorker(
    int thread_id,
    int provider_id,
    const ServiceWorkerRegistrationObjectInfo& info,
    const ServiceWorkerVersionAttributes& attrs) {
  DCHECK_EQ(kDocumentMainThreadId, thread_id);

  ProviderContextMap::iterator context = provider_contexts_.find(provider_id);
  if (context == provider_contexts_.end())
    return;
  context->second->OnAssociateRegistration(info, attrs);
}

void ServiceWorkerDispatcher::OnAssociateRegistration(
    int thread_id,
    int provider_id,
    const ServiceWorkerRegistrationObjectInfo& info,
    const ServiceWorkerVersionAttributes& attrs) {
  ProviderContextMap::iterator provider = provider_contexts_.find(provider_id);
  if (provider == provider_contexts_.end())
    return;
  provider->second->OnAssociateRegistration(info, attrs);
}

void ServiceWorkerDispatcher::OnDisassociateRegistration(
    int thread_id,
    int provider_id) {
  ProviderContextMap::iterator provider = provider_contexts_.find(provider_id);
  if (provider == provider_contexts_.end())
    return;
  provider->second->OnDisassociateRegistration();
}

void ServiceWorkerDispatcher::OnRegistered(
    int thread_id,
    int request_id,
    const ServiceWorkerRegistrationObjectInfo& info,
    const ServiceWorkerVersionAttributes& attrs) {
  TRACE_EVENT_ASYNC_STEP_INTO0("ServiceWorker",
                               "ServiceWorkerDispatcher::RegisterServiceWorker",
                               request_id,
                               "OnRegistered");
  TRACE_EVENT_ASYNC_END0("ServiceWorker",
                         "ServiceWorkerDispatcher::RegisterServiceWorker",
                         request_id);
  WebServiceWorkerRegistrationCallbacks* callbacks =
      pending_registration_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  if (!callbacks)
    return;

  callbacks->onSuccess(WebServiceWorkerRegistrationImpl::CreateHandle(
      GetOrAdoptRegistration(info, attrs)));
  pending_registration_callbacks_.Remove(request_id);
}

void ServiceWorkerDispatcher::OnUpdated(int thread_id, int request_id) {
  TRACE_EVENT_ASYNC_STEP_INTO0("ServiceWorker",
                               "ServiceWorkerDispatcher::UpdateServiceWorker",
                               request_id, "OnUpdated");
  TRACE_EVENT_ASYNC_END0("ServiceWorker",
                         "ServiceWorkerDispatcher::UpdateServiceWorker",
                         request_id);
  WebServiceWorkerUpdateCallbacks* callbacks =
      pending_update_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  if (!callbacks)
    return;

  callbacks->onSuccess();
  pending_update_callbacks_.Remove(request_id);
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
  callbacks->onSuccess(is_success);
  pending_unregistration_callbacks_.Remove(request_id);
}

void ServiceWorkerDispatcher::OnDidGetRegistration(
    int thread_id,
    int request_id,
    const ServiceWorkerRegistrationObjectInfo& info,
    const ServiceWorkerVersionAttributes& attrs) {
  TRACE_EVENT_ASYNC_STEP_INTO0(
      "ServiceWorker",
      "ServiceWorkerDispatcher::GetRegistration",
      request_id,
      "OnDidGetRegistration");
  TRACE_EVENT_ASYNC_END0("ServiceWorker",
                         "ServiceWorkerDispatcher::GetRegistration",
                         request_id);
  WebServiceWorkerGetRegistrationCallbacks* callbacks =
      pending_get_registration_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  if (!callbacks)
    return;

  scoped_refptr<WebServiceWorkerRegistrationImpl> registration;
  if (info.handle_id != kInvalidServiceWorkerRegistrationHandleId)
    registration = GetOrAdoptRegistration(info, attrs);

  callbacks->onSuccess(
      WebServiceWorkerRegistrationImpl::CreateHandle(registration));
  pending_get_registration_callbacks_.Remove(request_id);
}

void ServiceWorkerDispatcher::OnDidGetRegistrations(
    int thread_id,
    int request_id,
    const std::vector<ServiceWorkerRegistrationObjectInfo>& infos,
    const std::vector<ServiceWorkerVersionAttributes>& attrs) {
  TRACE_EVENT_ASYNC_STEP_INTO0(
      "ServiceWorker",
      "ServiceWorkerDispatcher::GetRegistrations",
      request_id,
      "OnDidGetRegistrations");
  TRACE_EVENT_ASYNC_END0("ServiceWorker",
                         "ServiceWorkerDispatcher::GetRegistrations",
                         request_id);

  WebServiceWorkerGetRegistrationsCallbacks* callbacks =
      pending_get_registrations_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  if (!callbacks)
    return;

  typedef blink::WebVector<blink::WebServiceWorkerRegistration::Handle*>
      WebServiceWorkerRegistrationArray;
  scoped_ptr<WebServiceWorkerRegistrationArray> registrations(
      new WebServiceWorkerRegistrationArray(infos.size()));
  for (size_t i = 0; i < infos.size(); ++i) {
    if (infos[i].handle_id != kInvalidServiceWorkerHandleId) {
      ServiceWorkerRegistrationObjectInfo info(infos[i]);
      ServiceWorkerVersionAttributes attr(attrs[i]);

      // WebServiceWorkerGetRegistrationsCallbacks cannot receive an array of
      // WebPassOwnPtr<WebServiceWorkerRegistration::Handle>, so create leaky
      // handles instead.
      (*registrations)[i] = WebServiceWorkerRegistrationImpl::CreateLeakyHandle(
          GetOrAdoptRegistration(info, attr));
    }
  }

  callbacks->onSuccess(blink::adoptWebPtr(registrations.release()));
  pending_get_registrations_callbacks_.Remove(request_id);
}

void ServiceWorkerDispatcher::OnDidGetRegistrationForReady(
    int thread_id,
    int request_id,
    const ServiceWorkerRegistrationObjectInfo& info,
    const ServiceWorkerVersionAttributes& attrs) {
  TRACE_EVENT_ASYNC_STEP_INTO0(
      "ServiceWorker",
      "ServiceWorkerDispatcher::GetRegistrationForReady",
      request_id,
      "OnDidGetRegistrationForReady");
  TRACE_EVENT_ASYNC_END0("ServiceWorker",
                         "ServiceWorkerDispatcher::GetRegistrationForReady",
                         request_id);
  WebServiceWorkerGetRegistrationForReadyCallbacks* callbacks =
      get_for_ready_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  if (!callbacks)
    return;

  callbacks->onSuccess(WebServiceWorkerRegistrationImpl::CreateHandle(
      GetOrAdoptRegistration(info, attrs)));
  get_for_ready_callbacks_.Remove(request_id);
}

void ServiceWorkerDispatcher::OnRegistrationError(
    int thread_id,
    int request_id,
    WebServiceWorkerError::ErrorType error_type,
    const base::string16& message) {
  TRACE_EVENT_ASYNC_STEP_INTO0("ServiceWorker",
                               "ServiceWorkerDispatcher::RegisterServiceWorker",
                               request_id,
                               "OnRegistrationError");
  TRACE_EVENT_ASYNC_END0("ServiceWorker",
                         "ServiceWorkerDispatcher::RegisterServiceWorker",
                         request_id);
  WebServiceWorkerRegistrationCallbacks* callbacks =
      pending_registration_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  if (!callbacks)
    return;

  callbacks->onError(WebServiceWorkerError(error_type, message));
  pending_registration_callbacks_.Remove(request_id);
}

void ServiceWorkerDispatcher::OnUpdateError(
    int thread_id,
    int request_id,
    WebServiceWorkerError::ErrorType error_type,
    const base::string16& message) {
  TRACE_EVENT_ASYNC_STEP_INTO0("ServiceWorker",
                               "ServiceWorkerDispatcher::UpdateServiceWorker",
                               request_id, "OnUpdateError");
  TRACE_EVENT_ASYNC_END0("ServiceWorker",
                         "ServiceWorkerDispatcher::UpdateServiceWorker",
                         request_id);
  WebServiceWorkerUpdateCallbacks* callbacks =
      pending_update_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  if (!callbacks)
    return;

  callbacks->onError(WebServiceWorkerError(error_type, message));
  pending_update_callbacks_.Remove(request_id);
}

void ServiceWorkerDispatcher::OnUnregistrationError(
    int thread_id,
    int request_id,
    WebServiceWorkerError::ErrorType error_type,
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

  callbacks->onError(WebServiceWorkerError(error_type, message));
  pending_unregistration_callbacks_.Remove(request_id);
}

void ServiceWorkerDispatcher::OnGetRegistrationError(
    int thread_id,
    int request_id,
    WebServiceWorkerError::ErrorType error_type,
    const base::string16& message) {
  TRACE_EVENT_ASYNC_STEP_INTO0(
      "ServiceWorker",
      "ServiceWorkerDispatcher::GetRegistration",
      request_id,
      "OnGetRegistrationError");
  TRACE_EVENT_ASYNC_END0("ServiceWorker",
                         "ServiceWorkerDispatcher::GetRegistration",
                         request_id);
  WebServiceWorkerGetRegistrationCallbacks* callbacks =
      pending_get_registration_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  if (!callbacks)
    return;

  callbacks->onError(WebServiceWorkerError(error_type, message));
  pending_get_registration_callbacks_.Remove(request_id);
}

void ServiceWorkerDispatcher::OnGetRegistrationsError(
    int thread_id,
    int request_id,
    WebServiceWorkerError::ErrorType error_type,
    const base::string16& message) {
  TRACE_EVENT_ASYNC_STEP_INTO0(
      "ServiceWorker",
      "ServiceWorkerDispatcher::GetRegistrations",
      request_id,
      "OnGetRegistrationsError");
  TRACE_EVENT_ASYNC_END0("ServiceWorker",
                         "ServiceWorkerDispatcher::GetRegistrations",
                         request_id);
  WebServiceWorkerGetRegistrationsCallbacks* callbacks =
      pending_get_registrations_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  if (!callbacks)
    return;

  callbacks->onError(WebServiceWorkerError(error_type, message));
  pending_get_registrations_callbacks_.Remove(request_id);
}

void ServiceWorkerDispatcher::OnServiceWorkerStateChanged(
    int thread_id,
    int handle_id,
    blink::WebServiceWorkerState state) {
  TRACE_EVENT2("ServiceWorker",
               "ServiceWorkerDispatcher::OnServiceWorkerStateChanged",
               "Thread ID", thread_id,
               "State", state);
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

  RegistrationObjectMap::iterator found =
      registrations_.find(registration_handle_id);
  if (found != registrations_.end()) {
    // Populate the version fields (eg. .installing) with new worker objects.
    ChangedVersionAttributesMask mask(changed_mask);
    if (mask.installing_changed())
      found->second->SetInstalling(GetOrAdoptServiceWorker(attrs.installing));
    if (mask.waiting_changed())
      found->second->SetWaiting(GetOrAdoptServiceWorker(attrs.waiting));
    if (mask.active_changed())
      found->second->SetActive(GetOrAdoptServiceWorker(attrs.active));
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
    int thread_id,
    int provider_id,
    const ServiceWorkerObjectInfo& info,
    bool should_notify_controllerchange) {
  TRACE_EVENT2("ServiceWorker",
               "ServiceWorkerDispatcher::OnSetControllerServiceWorker",
               "Thread ID", thread_id,
               "Provider ID", provider_id);

  ProviderContextMap::iterator provider = provider_contexts_.find(provider_id);
  if (provider != provider_contexts_.end())
    provider->second->OnSetControllerServiceWorker(info);

  ProviderClientMap::iterator found = provider_clients_.find(provider_id);
  if (found != provider_clients_.end()) {
    // Populate the .controller field with the new worker object.
    scoped_refptr<WebServiceWorkerImpl> worker = GetOrAdoptServiceWorker(info);
    found->second->setController(WebServiceWorkerImpl::CreateHandle(worker),
                                 should_notify_controllerchange);
  }
}

void ServiceWorkerDispatcher::OnPostMessage(
    const ServiceWorkerMsg_MessageToDocument_Params& params) {
  // Make sure we're on the main document thread. (That must be the only
  // thread we get this message)
  DCHECK(ChildThreadImpl::current());
  TRACE_EVENT1("ServiceWorker", "ServiceWorkerDispatcher::OnPostMessage",
               "Thread ID", params.thread_id);

  ProviderClientMap::iterator found =
      provider_clients_.find(params.provider_id);
  if (found == provider_clients_.end()) {
    // For now we do no queueing for messages sent to nonexistent / unattached
    // client.
    return;
  }

  blink::WebMessagePortChannelArray ports =
      WebMessagePortChannelImpl::CreatePorts(
          params.message_ports, params.new_routing_ids,
          base::ThreadTaskRunnerHandle::Get());

  scoped_refptr<WebServiceWorkerImpl> worker =
      GetOrAdoptServiceWorker(params.service_worker_info);
  found->second->dispatchMessageEvent(
      WebServiceWorkerImpl::CreateHandle(worker), params.message, ports);
}

void ServiceWorkerDispatcher::AddServiceWorker(
    int handle_id, WebServiceWorkerImpl* worker) {
  DCHECK(!ContainsKey(service_workers_, handle_id));
  service_workers_[handle_id] = worker;
}

void ServiceWorkerDispatcher::RemoveServiceWorker(int handle_id) {
  DCHECK(ContainsKey(service_workers_, handle_id));
  service_workers_.erase(handle_id);
}

void ServiceWorkerDispatcher::AddServiceWorkerRegistration(
    int registration_handle_id,
    WebServiceWorkerRegistrationImpl* registration) {
  DCHECK(!ContainsKey(registrations_, registration_handle_id));
  registrations_[registration_handle_id] = registration;
}

void ServiceWorkerDispatcher::RemoveServiceWorkerRegistration(
    int registration_handle_id) {
  DCHECK(ContainsKey(registrations_, registration_handle_id));
  registrations_.erase(registration_handle_id);
}

}  // namespace content
