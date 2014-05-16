// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_dispatcher_host.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/message_port_message_filter.h"
#include "content/browser/message_port_service.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_handle.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_utils.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerError.h"
#include "url/gurl.h"

using blink::WebServiceWorkerError;

namespace content {

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

ServiceWorkerDispatcherHost::ServiceWorkerDispatcherHost(
    int render_process_id,
    MessagePortMessageFilter* message_port_message_filter)
    : BrowserMessageFilter(kFilteredMessageClasses,
                           arraysize(kFilteredMessageClasses)),
      render_process_id_(render_process_id),
      message_port_message_filter_(message_port_message_filter),
      channel_ready_(false) {
}

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

void ServiceWorkerDispatcherHost::OnFilterAdded(IPC::Channel* channel) {
  BrowserMessageFilter::OnFilterAdded(channel);
  channel_ready_ = true;
  std::vector<IPC::Message*> messages;
  pending_messages_.release(&messages);
  for (size_t i = 0; i < messages.size(); ++i) {
    BrowserMessageFilter::Send(messages[i]);
  }
}

void ServiceWorkerDispatcherHost::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

bool ServiceWorkerDispatcherHost::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ServiceWorkerDispatcherHost, message)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_RegisterServiceWorker,
                        OnRegisterServiceWorker)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_UnregisterServiceWorker,
                        OnUnregisterServiceWorker)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_ProviderCreated,
                        OnProviderCreated)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_ProviderDestroyed,
                        OnProviderDestroyed)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_SetVersionId,
                        OnSetHostedVersionId)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_PostMessageToWorker,
                        OnPostMessageToWorker)
    IPC_MESSAGE_HANDLER(EmbeddedWorkerHostMsg_WorkerScriptLoaded,
                        OnWorkerScriptLoaded)
    IPC_MESSAGE_HANDLER(EmbeddedWorkerHostMsg_WorkerScriptLoadFailed,
                        OnWorkerScriptLoadFailed)
    IPC_MESSAGE_HANDLER(EmbeddedWorkerHostMsg_WorkerStarted,
                        OnWorkerStarted)
    IPC_MESSAGE_HANDLER(EmbeddedWorkerHostMsg_WorkerStopped,
                        OnWorkerStopped)
    IPC_MESSAGE_HANDLER(EmbeddedWorkerHostMsg_ReportException,
                        OnReportException)
    IPC_MESSAGE_HANDLER(EmbeddedWorkerHostMsg_ReportConsoleMessage,
                        OnReportConsoleMessage)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_IncrementServiceWorkerRefCount,
                        OnIncrementServiceWorkerRefCount)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount,
                        OnDecrementServiceWorkerRefCount)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  if (!handled && context_) {
    handled = context_->embedded_worker_registry()->OnMessageReceived(message);
    if (!handled)
      BadMessageReceived();
  }

  return handled;
}

bool ServiceWorkerDispatcherHost::Send(IPC::Message* message) {
  if (channel_ready_) {
    BrowserMessageFilter::Send(message);
    // Don't bother passing through Send()'s result: it's not reliable.
    return true;
  }

  pending_messages_.push_back(message);
  return true;
}

void ServiceWorkerDispatcherHost::RegisterServiceWorkerHandle(
    scoped_ptr<ServiceWorkerHandle> handle) {
  int handle_id = handle->handle_id();
  handles_.AddWithID(handle.release(), handle_id);
}

void ServiceWorkerDispatcherHost::OnRegisterServiceWorker(
    int thread_id,
    int request_id,
    int provider_id,
    const GURL& pattern,
    const GURL& script_url) {
  if (!context_ || !ServiceWorkerUtils::IsFeatureEnabled()) {
    Send(new ServiceWorkerMsg_ServiceWorkerRegistrationError(
        thread_id,
        request_id,
        WebServiceWorkerError::ErrorTypeDisabled,
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
        WebServiceWorkerError::ErrorTypeSecurity,
        base::ASCIIToUTF16(kDomainMismatchErrorMessage)));
    return;
  }

  ServiceWorkerProviderHost* provider_host = context_->GetProviderHost(
      render_process_id_, provider_id);
  if (!provider_host) {
    BadMessageReceived();
    return;
  }

  context_->RegisterServiceWorker(
      pattern,
      script_url,
      render_process_id_,
      provider_host,
      base::Bind(&ServiceWorkerDispatcherHost::RegistrationComplete,
                 this,
                 thread_id,
                 request_id));
}

void ServiceWorkerDispatcherHost::OnUnregisterServiceWorker(
    int thread_id,
    int request_id,
    int provider_id,
    const GURL& pattern) {
  // TODO(alecflett): This check is insufficient for release. Add a
  // ServiceWorker-specific policy query in
  // ChildProcessSecurityImpl. See http://crbug.com/311631.
  if (!context_ || !ServiceWorkerUtils::IsFeatureEnabled()) {
    Send(new ServiceWorkerMsg_ServiceWorkerRegistrationError(
        thread_id,
        request_id,
        blink::WebServiceWorkerError::ErrorTypeDisabled,
        base::ASCIIToUTF16(kDisabledErrorMessage)));
    return;
  }

  ServiceWorkerProviderHost* provider_host = context_->GetProviderHost(
      render_process_id_, provider_id);
  if (!provider_host) {
    BadMessageReceived();
    return;
  }

  context_->UnregisterServiceWorker(
      pattern,
      base::Bind(&ServiceWorkerDispatcherHost::UnregistrationComplete,
                 this,
                 thread_id,
                 request_id));
}

void ServiceWorkerDispatcherHost::OnPostMessageToWorker(
    int handle_id,
    const base::string16& message,
    const std::vector<int>& sent_message_port_ids) {
  if (!context_ || !ServiceWorkerUtils::IsFeatureEnabled())
    return;

  ServiceWorkerHandle* handle = handles_.Lookup(handle_id);
  if (!handle) {
    BadMessageReceived();
    return;
  }

  std::vector<int> new_routing_ids;
  message_port_message_filter_->UpdateMessagePortsWithNewRoutes(
      sent_message_port_ids, &new_routing_ids);
  handle->version()->SendMessage(
      ServiceWorkerMsg_MessageToWorker(message,
                                       sent_message_port_ids,
                                       new_routing_ids),
      base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));
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
          render_process_id_, provider_id, context_, this));
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
    int thread_id,
    int request_id,
    ServiceWorkerStatusCode status,
    int64 registration_id,
    int64 version_id) {
  if (!context_)
    return;

  if (status != SERVICE_WORKER_OK) {
    SendRegistrationError(thread_id, request_id, status);
    return;
  }

  ServiceWorkerVersion* version = context_->GetLiveVersion(version_id);
  DCHECK(version);
  DCHECK_EQ(registration_id, version->registration_id());
  scoped_ptr<ServiceWorkerHandle> handle =
      ServiceWorkerHandle::Create(context_, this, thread_id, version);
  Send(new ServiceWorkerMsg_ServiceWorkerRegistered(
      thread_id, request_id, handle->GetObjectInfo()));
  RegisterServiceWorkerHandle(handle.Pass());
}

void ServiceWorkerDispatcherHost::OnWorkerScriptLoaded(int embedded_worker_id) {
  if (!context_)
    return;
  context_->embedded_worker_registry()->OnWorkerScriptLoaded(
      render_process_id_, embedded_worker_id);
}

void ServiceWorkerDispatcherHost::OnWorkerScriptLoadFailed(
    int embedded_worker_id) {
  if (!context_)
    return;
  context_->embedded_worker_registry()->OnWorkerScriptLoadFailed(
      render_process_id_, embedded_worker_id);
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

void ServiceWorkerDispatcherHost::OnReportException(
    int embedded_worker_id,
    const base::string16& error_message,
    int line_number,
    int column_number,
    const GURL& source_url) {
  if (!context_)
    return;
  context_->embedded_worker_registry()->OnReportException(embedded_worker_id,
                                                          error_message,
                                                          line_number,
                                                          column_number,
                                                          source_url);
}

void ServiceWorkerDispatcherHost::OnReportConsoleMessage(
    int embedded_worker_id,
    const EmbeddedWorkerHostMsg_ReportConsoleMessage_Params& params) {
  if (!context_)
    return;
  context_->embedded_worker_registry()->OnReportConsoleMessage(
      embedded_worker_id,
      params.source_identifier,
      params.message_level,
      params.message,
      params.line_number,
      params.source_url);
}

void ServiceWorkerDispatcherHost::OnIncrementServiceWorkerRefCount(
    int handle_id) {
  ServiceWorkerHandle* handle = handles_.Lookup(handle_id);
  if (!handle) {
    BadMessageReceived();
    return;
  }
  handle->IncrementRefCount();
}

void ServiceWorkerDispatcherHost::OnDecrementServiceWorkerRefCount(
    int handle_id) {
  ServiceWorkerHandle* handle = handles_.Lookup(handle_id);
  if (!handle) {
    BadMessageReceived();
    return;
  }
  handle->DecrementRefCount();
  if (handle->HasNoRefCount())
    handles_.Remove(handle_id);
}

void ServiceWorkerDispatcherHost::UnregistrationComplete(
    int thread_id,
    int request_id,
    ServiceWorkerStatusCode status) {
  if (status != SERVICE_WORKER_OK) {
    SendRegistrationError(thread_id, request_id, status);
    return;
  }

  Send(new ServiceWorkerMsg_ServiceWorkerUnregistered(thread_id, request_id));
}

void ServiceWorkerDispatcherHost::SendRegistrationError(
    int thread_id,
    int request_id,
    ServiceWorkerStatusCode status) {
  base::string16 error_message;
  blink::WebServiceWorkerError::ErrorType error_type;
  GetServiceWorkerRegistrationStatusResponse(
      status, &error_type, &error_message);
  Send(new ServiceWorkerMsg_ServiceWorkerRegistrationError(
      thread_id, request_id, error_type, error_message));
}

}  // namespace content
