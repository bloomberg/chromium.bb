// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_dispatcher_host.h"

#include "base/strings/utf_string_conversions.h"
#include "content/browser/message_port_message_filter.h"
#include "content/browser/message_port_service.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_utils.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerError.h"
#include "url/gurl.h"

using blink::WebServiceWorkerError;

namespace {

const char kDisabledErrorMessage[] =
    "ServiceWorker is disabled";
const char kDomainMismatchErrorMessage[] =
    "Scope and scripts do not have the same origin";

const uint32 kFilteredMessageClasses[] = {
  ServiceWorkerMsgStart,
  EmbeddedWorkerMsgStart,
};

}  // namespace

namespace content {

ServiceWorkerDispatcherHost::ServiceWorkerDispatcherHost(
    int render_process_id,
    MessagePortMessageFilter* message_port_message_filter)
    : BrowserMessageFilter(kFilteredMessageClasses,
                           arraysize(kFilteredMessageClasses)),
      render_process_id_(render_process_id),
      message_port_message_filter_(message_port_message_filter) {}

ServiceWorkerDispatcherHost::~ServiceWorkerDispatcherHost() {
  if (context_) {
    context_->RemoveAllProviderHostsForProcess(render_process_id_);
    context_->embedded_worker_registry()->RemoveChildProcessSender(
        render_process_id_);
  }
}

void ServiceWorkerDispatcherHost::Init(
    ServiceWorkerContextWrapper* context_wrapper) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ServiceWorkerDispatcherHost::Init,
                    this, make_scoped_refptr(context_wrapper)));
      return;
  }
  context_ = context_wrapper->context()->AsWeakPtr();
  context_->embedded_worker_registry()->AddChildProcessSender(
      render_process_id_, this);
}

void ServiceWorkerDispatcherHost::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

bool ServiceWorkerDispatcherHost::OnMessageReceived(
    const IPC::Message& message,
    bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(
    ServiceWorkerDispatcherHost, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_RegisterServiceWorker,
                        OnRegisterServiceWorker)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_UnregisterServiceWorker,
                        OnUnregisterServiceWorker)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_ProviderCreated,
                        OnProviderCreated)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_ProviderDestroyed,
                        OnProviderDestroyed)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_AddScriptClient,
                        OnAddScriptClient)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_RemoveScriptClient,
                        OnRemoveScriptClient)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_SetVersionId,
                        OnSetHostedVersionId)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_PostMessage, OnPostMessage)
    IPC_MESSAGE_HANDLER(EmbeddedWorkerHostMsg_WorkerStarted,
                        OnWorkerStarted)
    IPC_MESSAGE_HANDLER(EmbeddedWorkerHostMsg_WorkerStopped,
                        OnWorkerStopped)
    IPC_MESSAGE_HANDLER(EmbeddedWorkerHostMsg_SendMessageToBrowser,
                        OnSendMessageToBrowser)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void ServiceWorkerDispatcherHost::OnRegisterServiceWorker(
    int32 thread_id,
    int32 request_id,
    const GURL& pattern,
    const GURL& script_url) {
  if (!context_ || !ServiceWorkerUtils::IsFeatureEnabled()) {
    Send(new ServiceWorkerMsg_ServiceWorkerRegistrationError(
        thread_id,
        request_id,
        WebServiceWorkerError::DisabledError,
        base::ASCIIToUTF16(kDisabledErrorMessage)));
    return;
  }

  // TODO(alecflett): This check is insufficient for release. Add a
  // ServiceWorker-specific policy query in
  // ChildProcessSecurityImpl. See http://crbug.com/311631.
  if (pattern.GetOrigin() != script_url.GetOrigin()) {
    Send(new ServiceWorkerMsg_ServiceWorkerRegistrationError(
        thread_id,
        request_id,
        WebServiceWorkerError::SecurityError,
        base::ASCIIToUTF16(kDomainMismatchErrorMessage)));
    return;
  }

  context_->RegisterServiceWorker(
      pattern,
      script_url,
      render_process_id_,
      base::Bind(&ServiceWorkerDispatcherHost::RegistrationComplete,
                 this,
                 thread_id,
                 request_id));
}

void ServiceWorkerDispatcherHost::OnUnregisterServiceWorker(
    int32 thread_id,
    int32 request_id,
    const GURL& pattern) {
  // TODO(alecflett): This check is insufficient for release. Add a
  // ServiceWorker-specific policy query in
  // ChildProcessSecurityImpl. See http://crbug.com/311631.
  if (!context_ || !ServiceWorkerUtils::IsFeatureEnabled()) {
    Send(new ServiceWorkerMsg_ServiceWorkerRegistrationError(
        thread_id,
        request_id,
        blink::WebServiceWorkerError::DisabledError,
        base::ASCIIToUTF16(kDisabledErrorMessage)));
    return;
  }

  context_->UnregisterServiceWorker(
      pattern,
      render_process_id_,
      base::Bind(&ServiceWorkerDispatcherHost::UnregistrationComplete,
                 this,
                 thread_id,
                 request_id));
}

void ServiceWorkerDispatcherHost::OnPostMessage(
    int64 registration_id,
    const base::string16& message,
    const std::vector<int>& sent_message_port_ids) {
  if (!context_ || !ServiceWorkerUtils::IsFeatureEnabled())
    return;

  std::vector<int> new_routing_ids(sent_message_port_ids.size());
  for (size_t i = 0; i < sent_message_port_ids.size(); ++i) {
    new_routing_ids[i] = message_port_message_filter_->GetNextRoutingID();
    MessagePortService::GetInstance()->UpdateMessagePort(
        sent_message_port_ids[i],
        message_port_message_filter_,
        new_routing_ids[i]);
  }

  context_->storage()->FindRegistrationForId(
      registration_id,
      base::Bind(&ServiceWorkerDispatcherHost::PostMessageFoundRegistration,
                 message,
                 sent_message_port_ids,
                 new_routing_ids));
}

namespace {
void NoOpStatusCallback(ServiceWorkerStatusCode status) {}
}  // namespace

// static
void ServiceWorkerDispatcherHost::PostMessageFoundRegistration(
    const base::string16& message,
    const std::vector<int>& sent_message_port_ids,
    const std::vector<int>& new_routing_ids,
    ServiceWorkerStatusCode status,
    const scoped_refptr<ServiceWorkerRegistration>& result) {
  if (status != SERVICE_WORKER_OK)
    return;
  DCHECK(result);

  // TODO(jsbell): Route message to appropriate version. crbug.com/351797
  ServiceWorkerVersion* version = result->active_version();
  if (!version)
    return;
  version->SendMessage(
      ServiceWorkerMsg_Message(message, sent_message_port_ids, new_routing_ids),
      base::Bind(&NoOpStatusCallback));
}

void ServiceWorkerDispatcherHost::OnProviderCreated(int provider_id) {
  if (!context_)
    return;
  if (context_->GetProviderHost(render_process_id_, provider_id)) {
    BadMessageReceived();
    return;
  }
  scoped_ptr<ServiceWorkerProviderHost> provider_host(
      new ServiceWorkerProviderHost(
          render_process_id_, provider_id, context_));
  context_->AddProviderHost(provider_host.Pass());
}

void ServiceWorkerDispatcherHost::OnProviderDestroyed(int provider_id) {
  if (!context_)
    return;
  if (!context_->GetProviderHost(render_process_id_, provider_id)) {
    BadMessageReceived();
    return;
  }
  context_->RemoveProviderHost(render_process_id_, provider_id);
}

void ServiceWorkerDispatcherHost::OnAddScriptClient(
    int thread_id, int provider_id) {
  if (!context_)
    return;
  ServiceWorkerProviderHost* provider_host =
      context_->GetProviderHost(render_process_id_, provider_id);
  if (!provider_host)
    return;
  provider_host->AddScriptClient(thread_id);
}

void ServiceWorkerDispatcherHost::OnRemoveScriptClient(
    int thread_id, int provider_id) {
  if (!context_)
    return;
  ServiceWorkerProviderHost* provider_host =
      context_->GetProviderHost(render_process_id_, provider_id);
  if (!provider_host)
    return;
  provider_host->RemoveScriptClient(thread_id);
}

void ServiceWorkerDispatcherHost::OnSetHostedVersionId(
    int provider_id, int64 version_id) {
  if (!context_)
    return;
  ServiceWorkerProviderHost* provider_host =
      context_->GetProviderHost(render_process_id_, provider_id);
  if (!provider_host || !provider_host->SetHostedVersionId(version_id)) {
    BadMessageReceived();
    return;
  }
}

void ServiceWorkerDispatcherHost::RegistrationComplete(
    int32 thread_id,
    int32 request_id,
    ServiceWorkerStatusCode status,
    int64 registration_id) {
  if (status != SERVICE_WORKER_OK) {
    SendRegistrationError(thread_id, request_id, status);
    return;
  }

  Send(new ServiceWorkerMsg_ServiceWorkerRegistered(
      thread_id, request_id, registration_id));
}

void ServiceWorkerDispatcherHost::OnWorkerStarted(
    int thread_id, int embedded_worker_id) {
  if (!context_)
    return;
  context_->embedded_worker_registry()->OnWorkerStarted(
      render_process_id_, thread_id, embedded_worker_id);
}

void ServiceWorkerDispatcherHost::OnWorkerStopped(int embedded_worker_id) {
  if (!context_)
    return;
  context_->embedded_worker_registry()->OnWorkerStopped(
      render_process_id_, embedded_worker_id);
}

void ServiceWorkerDispatcherHost::OnSendMessageToBrowser(
    int embedded_worker_id,
    int request_id,
    const IPC::Message& message) {
  if (!context_)
    return;
  context_->embedded_worker_registry()->OnSendMessageToBrowser(
      embedded_worker_id, request_id, message);
}

void ServiceWorkerDispatcherHost::UnregistrationComplete(
    int32 thread_id,
    int32 request_id,
    ServiceWorkerStatusCode status) {
  if (status != SERVICE_WORKER_OK) {
    SendRegistrationError(thread_id, request_id, status);
    return;
  }

  Send(new ServiceWorkerMsg_ServiceWorkerUnregistered(thread_id, request_id));
}

void ServiceWorkerDispatcherHost::SendRegistrationError(
    int32 thread_id,
    int32 request_id,
    ServiceWorkerStatusCode status) {
  base::string16 error_message;
  blink::WebServiceWorkerError::ErrorType error_type;
  GetServiceWorkerRegistrationStatusResponse(
      status, &error_type, &error_message);
  Send(new ServiceWorkerMsg_ServiceWorkerRegistrationError(
      thread_id, request_id, error_type, error_message));
}

}  // namespace content
