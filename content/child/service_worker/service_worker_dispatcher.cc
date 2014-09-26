// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/service_worker/service_worker_dispatcher.h"

#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "base/stl_util.h"
#include "base/threading/thread_local.h"
#include "content/child/child_thread.h"
#include "content/child/service_worker/service_worker_handle_reference.h"
#include "content/child/service_worker/service_worker_provider_context.h"
#include "content/child/service_worker/service_worker_registration_handle_reference.h"
#include "content/child/service_worker/web_service_worker_impl.h"
#include "content/child/service_worker/web_service_worker_registration_impl.h"
#include "content/child/thread_safe_sender.h"
#include "content/child/webmessageportchannel_impl.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/public/common/url_utils.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerProviderClient.h"
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
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_AssociateRegistration,
                        OnAssociateRegistration)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_DisassociateRegistration,
                        OnDisassociateRegistration)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ServiceWorkerRegistered, OnRegistered)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ServiceWorkerUnregistered,
                        OnUnregistered)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_DidGetRegistration,
                        OnDidGetRegistration)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ServiceWorkerRegistrationError,
                        OnRegistrationError)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ServiceWorkerUnregistrationError,
                        OnUnregistrationError)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ServiceWorkerGetRegistrationError,
                        OnGetRegistrationError)
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
    scoped_ptr<WebServiceWorkerError> error(new WebServiceWorkerError(
        WebServiceWorkerError::ErrorTypeSecurity, "URL too long"));
    callbacks->onError(error.release());
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

void ServiceWorkerDispatcher::UnregisterServiceWorker(
    int provider_id,
    const GURL& pattern,
    WebServiceWorkerUnregistrationCallbacks* callbacks) {
  DCHECK(callbacks);

  if (pattern.possibly_invalid_spec().size() > GetMaxURLChars()) {
    scoped_ptr<WebServiceWorkerUnregistrationCallbacks>
        owned_callbacks(callbacks);
    scoped_ptr<WebServiceWorkerError> error(new WebServiceWorkerError(
        WebServiceWorkerError::ErrorTypeSecurity, "URL too long"));
    callbacks->onError(error.release());
    return;
  }

  int request_id = pending_unregistration_callbacks_.Add(callbacks);
  TRACE_EVENT_ASYNC_BEGIN1("ServiceWorker",
                           "ServiceWorkerDispatcher::UnregisterServiceWorker",
                           request_id,
                           "Scope", pattern.spec());
  thread_safe_sender_->Send(new ServiceWorkerHostMsg_UnregisterServiceWorker(
      CurrentWorkerId(), request_id, provider_id, pattern));
}

void ServiceWorkerDispatcher::GetRegistration(
    int provider_id,
    const GURL& document_url,
    WebServiceWorkerRegistrationCallbacks* callbacks) {
  DCHECK(callbacks);

  if (document_url.possibly_invalid_spec().size() > GetMaxURLChars()) {
    scoped_ptr<WebServiceWorkerRegistrationCallbacks>
        owned_callbacks(callbacks);
    scoped_ptr<WebServiceWorkerError> error(new WebServiceWorkerError(
        WebServiceWorkerError::ErrorTypeSecurity, "URL too long"));
    callbacks->onError(error.release());
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
  worker_to_provider_.erase(provider_context->installing_handle_id());
  worker_to_provider_.erase(provider_context->waiting_handle_id());
  worker_to_provider_.erase(provider_context->active_handle_id());
  worker_to_provider_.erase(provider_context->controller_handle_id());
}

void ServiceWorkerDispatcher::AddScriptClient(
    int provider_id,
    blink::WebServiceWorkerProviderClient* client) {
  DCHECK(client);
  DCHECK(!ContainsKey(script_clients_, provider_id));
  script_clients_[provider_id] = client;
}

void ServiceWorkerDispatcher::RemoveScriptClient(int provider_id) {
  // This could be possibly called multiple times to ensure termination.
  if (ContainsKey(script_clients_, provider_id))
    script_clients_.erase(provider_id);
}

ServiceWorkerDispatcher*
ServiceWorkerDispatcher::GetOrCreateThreadSpecificInstance(
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

ServiceWorkerDispatcher* ServiceWorkerDispatcher::GetThreadSpecificInstance() {
  if (g_dispatcher_tls.Pointer()->Get() == kHasBeenDeleted)
    return NULL;
  return g_dispatcher_tls.Pointer()->Get();
}

void ServiceWorkerDispatcher::OnWorkerRunLoopStopped() {
  delete this;
}

WebServiceWorkerImpl* ServiceWorkerDispatcher::GetServiceWorker(
    const ServiceWorkerObjectInfo& info,
    bool adopt_handle) {
  if (info.handle_id == kInvalidServiceWorkerHandleId)
    return NULL;

  WorkerObjectMap::iterator existing_worker =
      service_workers_.find(info.handle_id);

  if (existing_worker != service_workers_.end()) {
    if (adopt_handle) {
      // We are instructed to adopt a handle but we already have one, so
      // adopt and destroy a handle ref.
      ServiceWorkerHandleReference::Adopt(info, thread_safe_sender_.get());
    }
    return existing_worker->second;
  }

  scoped_ptr<ServiceWorkerHandleReference> handle_ref =
      adopt_handle
          ? ServiceWorkerHandleReference::Adopt(info, thread_safe_sender_.get())
          : ServiceWorkerHandleReference::Create(info,
                                                 thread_safe_sender_.get());
  // WebServiceWorkerImpl constructor calls AddServiceWorker.
  return new WebServiceWorkerImpl(handle_ref.Pass(), thread_safe_sender_.get());
}

WebServiceWorkerRegistrationImpl*
ServiceWorkerDispatcher::FindServiceWorkerRegistration(
    const ServiceWorkerRegistrationObjectInfo& info,
    bool adopt_handle) {
  RegistrationObjectMap::iterator registration =
      registrations_.find(info.handle_id);
  if (registration == registrations_.end())
    return NULL;
  if (adopt_handle) {
    // We are instructed to adopt a handle but we already have one, so
    // adopt and destroy a handle ref.
    ServiceWorkerRegistrationHandleReference::Adopt(
        info, thread_safe_sender_.get());
  }
  return registration->second;
}

WebServiceWorkerRegistrationImpl*
ServiceWorkerDispatcher::CreateServiceWorkerRegistration(
    const ServiceWorkerRegistrationObjectInfo& info,
    bool adopt_handle) {
  DCHECK(!FindServiceWorkerRegistration(info, adopt_handle));
  if (info.handle_id == kInvalidServiceWorkerRegistrationHandleId)
    return NULL;

  scoped_ptr<ServiceWorkerRegistrationHandleReference> handle_ref =
      adopt_handle ? ServiceWorkerRegistrationHandleReference::Adopt(
                         info, thread_safe_sender_.get())
                   : ServiceWorkerRegistrationHandleReference::Create(
                         info, thread_safe_sender_.get());

  // WebServiceWorkerRegistrationImpl constructor calls
  // AddServiceWorkerRegistration.
  return new WebServiceWorkerRegistrationImpl(handle_ref.Pass());
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
  if (attrs.installing.handle_id != kInvalidServiceWorkerHandleId)
    worker_to_provider_[attrs.installing.handle_id] = provider->second;
  if (attrs.waiting.handle_id != kInvalidServiceWorkerHandleId)
    worker_to_provider_[attrs.waiting.handle_id] = provider->second;
  if (attrs.active.handle_id != kInvalidServiceWorkerHandleId)
    worker_to_provider_[attrs.active.handle_id] = provider->second;
}

void ServiceWorkerDispatcher::OnDisassociateRegistration(
    int thread_id,
    int provider_id) {
  ProviderContextMap::iterator provider = provider_contexts_.find(provider_id);
  if (provider == provider_contexts_.end())
    return;
  provider->second->OnDisassociateRegistration();
  worker_to_provider_.erase(provider->second->installing_handle_id());
  worker_to_provider_.erase(provider->second->waiting_handle_id());
  worker_to_provider_.erase(provider->second->active_handle_id());
  worker_to_provider_.erase(provider->second->controller_handle_id());
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

  callbacks->onSuccess(FindOrCreateRegistration(info, attrs));
  pending_registration_callbacks_.Remove(request_id);
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
  callbacks->onSuccess(&is_success);
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
  WebServiceWorkerRegistrationCallbacks* callbacks =
      pending_get_registration_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  if (!callbacks)
    return;

  WebServiceWorkerRegistrationImpl* registration = NULL;
  if (info.handle_id != kInvalidServiceWorkerHandleId)
    registration = FindOrCreateRegistration(info, attrs);

  callbacks->onSuccess(registration);
  pending_get_registration_callbacks_.Remove(request_id);
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

  scoped_ptr<WebServiceWorkerError> error(
      new WebServiceWorkerError(error_type, message));
  callbacks->onError(error.release());
  pending_registration_callbacks_.Remove(request_id);
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

  scoped_ptr<WebServiceWorkerError> error(
      new WebServiceWorkerError(error_type, message));
  callbacks->onError(error.release());
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

  scoped_ptr<WebServiceWorkerError> error(
      new WebServiceWorkerError(error_type, message));
  callbacks->onError(error.release());
  pending_get_registration_callbacks_.Remove(request_id);
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

  WorkerToProviderMap::iterator provider = worker_to_provider_.find(handle_id);
  if (provider != worker_to_provider_.end())
    provider->second->OnServiceWorkerStateChanged(handle_id, state);
}

void ServiceWorkerDispatcher::OnSetVersionAttributes(
    int thread_id,
    int provider_id,
    int registration_handle_id,
    int changed_mask,
    const ServiceWorkerVersionAttributes& attributes) {
  TRACE_EVENT1("ServiceWorker",
               "ServiceWorkerDispatcher::OnSetVersionAttributes",
               "Thread ID", thread_id);
  ChangedVersionAttributesMask mask(changed_mask);
  if (mask.installing_changed()) {
    SetInstallingServiceWorker(provider_id,
                               registration_handle_id,
                               attributes.installing);
  }
  if (mask.waiting_changed()) {
    SetWaitingServiceWorker(provider_id,
                            registration_handle_id,
                            attributes.waiting);
  }
  if (mask.active_changed()) {
    SetActiveServiceWorker(provider_id,
                           registration_handle_id,
                           attributes.active);
    SetReadyRegistration(provider_id, registration_handle_id);
  }
}

void ServiceWorkerDispatcher::OnUpdateFound(
    int thread_id,
    const ServiceWorkerRegistrationObjectInfo& info) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerDispatcher::OnUpdateFound");
  RegistrationObjectMap::iterator found = registrations_.find(info.handle_id);
  if (found != registrations_.end())
    found->second->OnUpdateFound();
}

void ServiceWorkerDispatcher::SetInstallingServiceWorker(
    int provider_id,
    int registration_handle_id,
    const ServiceWorkerObjectInfo& info) {
  ProviderContextMap::iterator provider = provider_contexts_.find(provider_id);
  if (provider != provider_contexts_.end() &&
      provider->second->registration_handle_id() == registration_handle_id) {
    int existing_installing_id = provider->second->installing_handle_id();
    if (existing_installing_id != info.handle_id &&
        existing_installing_id != kInvalidServiceWorkerHandleId) {
      WorkerToProviderMap::iterator associated_provider =
          worker_to_provider_.find(existing_installing_id);
      DCHECK(associated_provider != worker_to_provider_.end());
      DCHECK(associated_provider->second->provider_id() == provider_id);
      worker_to_provider_.erase(associated_provider);
    }
    provider->second->OnSetInstallingServiceWorker(
        registration_handle_id, info);
    if (info.handle_id != kInvalidServiceWorkerHandleId)
      worker_to_provider_[info.handle_id] = provider->second;
  }

  RegistrationObjectMap::iterator found =
      registrations_.find(registration_handle_id);
  if (found != registrations_.end()) {
    // Populate the .installing field with the new worker object.
    found->second->SetInstalling(GetServiceWorker(info, false));
  }
}

void ServiceWorkerDispatcher::SetWaitingServiceWorker(
    int provider_id,
    int registration_handle_id,
    const ServiceWorkerObjectInfo& info) {
  ProviderContextMap::iterator provider = provider_contexts_.find(provider_id);
  if (provider != provider_contexts_.end() &&
      provider->second->registration_handle_id() == registration_handle_id) {
    int existing_waiting_id = provider->second->waiting_handle_id();
    if (existing_waiting_id != info.handle_id &&
        existing_waiting_id != kInvalidServiceWorkerHandleId) {
      WorkerToProviderMap::iterator associated_provider =
          worker_to_provider_.find(existing_waiting_id);
      DCHECK(associated_provider != worker_to_provider_.end());
      DCHECK(associated_provider->second->provider_id() == provider_id);
      worker_to_provider_.erase(associated_provider);
    }
    provider->second->OnSetWaitingServiceWorker(registration_handle_id, info);
    if (info.handle_id != kInvalidServiceWorkerHandleId)
      worker_to_provider_[info.handle_id] = provider->second;
  }

  RegistrationObjectMap::iterator found =
      registrations_.find(registration_handle_id);
  if (found != registrations_.end()) {
    // Populate the .waiting field with the new worker object.
    found->second->SetWaiting(GetServiceWorker(info, false));
  }
}

void ServiceWorkerDispatcher::SetActiveServiceWorker(
    int provider_id,
    int registration_handle_id,
    const ServiceWorkerObjectInfo& info) {
  ProviderContextMap::iterator provider = provider_contexts_.find(provider_id);
  if (provider != provider_contexts_.end() &&
      provider->second->registration_handle_id() == registration_handle_id) {
    int existing_active_id = provider->second->active_handle_id();
    if (existing_active_id != info.handle_id &&
        existing_active_id != kInvalidServiceWorkerHandleId) {
      WorkerToProviderMap::iterator associated_provider =
          worker_to_provider_.find(existing_active_id);
      DCHECK(associated_provider != worker_to_provider_.end());
      DCHECK(associated_provider->second->provider_id() == provider_id);
      worker_to_provider_.erase(associated_provider);
    }
    provider->second->OnSetActiveServiceWorker(registration_handle_id, info);
    if (info.handle_id != kInvalidServiceWorkerHandleId)
      worker_to_provider_[info.handle_id] = provider->second;
  }

  RegistrationObjectMap::iterator found =
      registrations_.find(registration_handle_id);
  if (found != registrations_.end()) {
    // Populate the .active field with the new worker object.
    found->second->SetActive(GetServiceWorker(info, false));
  }
}

void ServiceWorkerDispatcher::SetReadyRegistration(
    int provider_id,
    int registration_handle_id) {
  ProviderContextMap::iterator provider = provider_contexts_.find(provider_id);
  if (provider == provider_contexts_.end() ||
      provider->second->registration_handle_id() != registration_handle_id ||
      provider->second->active_handle_id() == kInvalidServiceWorkerHandleId) {
    return;
  }

  ScriptClientMap::iterator client = script_clients_.find(provider_id);
  if (client == script_clients_.end())
    return;

  ServiceWorkerRegistrationObjectInfo info =
      provider->second->registration()->info();
  WebServiceWorkerRegistrationImpl* registration =
      FindServiceWorkerRegistration(info, false);
  if (!registration) {
    registration = CreateServiceWorkerRegistration(info, false);
    ServiceWorkerVersionAttributes attrs =
        provider->second->GetVersionAttributes();
    registration->SetInstalling(GetServiceWorker(attrs.installing, false));
    registration->SetWaiting(GetServiceWorker(attrs.waiting, false));
    registration->SetActive(GetServiceWorker(attrs.active, false));
  }

  // Resolve the .ready promise with the registration object.
  client->second->setReadyRegistration(registration);
}

void ServiceWorkerDispatcher::OnSetControllerServiceWorker(
    int thread_id,
    int provider_id,
    const ServiceWorkerObjectInfo& info) {
  TRACE_EVENT2("ServiceWorker",
               "ServiceWorkerDispatcher::OnSetControllerServiceWorker",
               "Thread ID", thread_id,
               "Provider ID", provider_id);
  ProviderContextMap::iterator provider = provider_contexts_.find(provider_id);
  if (provider != provider_contexts_.end()) {
    provider->second->OnSetControllerServiceWorker(
        provider->second->registration_handle_id(), info);
    worker_to_provider_[info.handle_id] = provider->second;
  }

  ScriptClientMap::iterator found = script_clients_.find(provider_id);
  if (found != script_clients_.end()) {
    // Populate the .controller field with the new worker object.
    found->second->setController(GetServiceWorker(info, false));
  }
}

void ServiceWorkerDispatcher::OnPostMessage(
    int thread_id,
    int provider_id,
    const base::string16& message,
    const std::vector<int>& sent_message_port_ids,
    const std::vector<int>& new_routing_ids) {
  // Make sure we're on the main document thread. (That must be the only
  // thread we get this message)
  DCHECK(ChildThread::current());
  TRACE_EVENT1("ServiceWorker",
               "ServiceWorkerDispatcher::OnPostMessage",
               "Thread ID", thread_id);

  ScriptClientMap::iterator found = script_clients_.find(provider_id);
  if (found == script_clients_.end()) {
    // For now we do no queueing for messages sent to nonexistent / unattached
    // client.
    return;
  }

  std::vector<WebMessagePortChannelImpl*> ports;
  if (!sent_message_port_ids.empty()) {
    ports.resize(sent_message_port_ids.size());
    for (size_t i = 0; i < sent_message_port_ids.size(); ++i) {
      ports[i] = new WebMessagePortChannelImpl(
          new_routing_ids[i], sent_message_port_ids[i],
          base::MessageLoopProxy::current());
    }
  }

  found->second->dispatchMessageEvent(message, ports);
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

WebServiceWorkerRegistrationImpl*
ServiceWorkerDispatcher::FindOrCreateRegistration(
    const ServiceWorkerRegistrationObjectInfo& info,
    const ServiceWorkerVersionAttributes& attrs) {
  WebServiceWorkerRegistrationImpl* registration =
      FindServiceWorkerRegistration(info, true);
  if (!registration) {
    registration = CreateServiceWorkerRegistration(info, true);
    registration->SetInstalling(GetServiceWorker(attrs.installing, true));
    registration->SetWaiting(GetServiceWorker(attrs.waiting, true));
    registration->SetActive(GetServiceWorker(attrs.active, true));
  } else {
    // |registration| must already have version attributes, so adopt and destroy
    // handle refs for them.
    ServiceWorkerHandleReference::Adopt(
        attrs.installing, thread_safe_sender_.get());
    ServiceWorkerHandleReference::Adopt(
        attrs.waiting, thread_safe_sender_.get());
    ServiceWorkerHandleReference::Adopt(
        attrs.active, thread_safe_sender_.get());
  }
  return registration;
}

}  // namespace content
