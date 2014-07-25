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

const char kShutdownErrorMessage[] =
    "The Service Worker system has shutdown.";

const uint32 kFilteredMessageClasses[] = {
  ServiceWorkerMsgStart,
  EmbeddedWorkerMsgStart,
};

// TODO(dominicc): When crbug.com/362214 is fixed, make
// Can(R|Unr)egisterServiceWorker also check that these are secure
// origins to defend against compromised renderers.
bool CanRegisterServiceWorker(const GURL& document_url,
                              const GURL& pattern,
                              const GURL& script_url) {
  // TODO: Respect Chrome's content settings, if we add a setting for
  // controlling whether Service Worker is allowed.
  return document_url.GetOrigin() == pattern.GetOrigin() &&
         document_url.GetOrigin() == script_url.GetOrigin();
}

bool CanUnregisterServiceWorker(const GURL& document_url,
                                const GURL& pattern) {
  // TODO: Respect Chrome's content settings, if we add a setting for
  // controlling whether Service Worker is allowed.
  return document_url.GetOrigin() == pattern.GetOrigin();
}

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
  if (GetContext()) {
    GetContext()->RemoveAllProviderHostsForProcess(render_process_id_);
    GetContext()->embedded_worker_registry()->RemoveChildProcessSender(
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
  context_wrapper_ = context_wrapper;
  GetContext()->embedded_worker_registry()->AddChildProcessSender(
      render_process_id_, this);
}

void ServiceWorkerDispatcherHost::OnFilterAdded(IPC::Sender* sender) {
  BrowserMessageFilter::OnFilterAdded(sender);
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
    IPC_MESSAGE_HANDLER(EmbeddedWorkerHostMsg_DidPauseAfterDownload,
                        OnPausedAfterDownload)
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

  if (!handled && GetContext()) {
    handled =
        GetContext()->embedded_worker_registry()->OnMessageReceived(message);
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
  if (!GetContext()) {
    Send(new ServiceWorkerMsg_ServiceWorkerRegistrationError(
        thread_id,
        request_id,
        WebServiceWorkerError::ErrorTypeAbort,
        base::ASCIIToUTF16(kShutdownErrorMessage)));
    return;
  }

  ServiceWorkerProviderHost* provider_host = GetContext()->GetProviderHost(
      render_process_id_, provider_id);
  if (!provider_host) {
    BadMessageReceived();
    return;
  }
  if (!provider_host->IsContextAlive()) {
    Send(new ServiceWorkerMsg_ServiceWorkerRegistrationError(
        thread_id,
        request_id,
        WebServiceWorkerError::ErrorTypeAbort,
        base::ASCIIToUTF16(kShutdownErrorMessage)));
    return;
  }

  if (!CanRegisterServiceWorker(
      provider_host->document_url(), pattern, script_url)) {
    BadMessageReceived();
    return;
  }
  GetContext()->RegisterServiceWorker(
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
  if (!GetContext()) {
    Send(new ServiceWorkerMsg_ServiceWorkerRegistrationError(
        thread_id,
        request_id,
        blink::WebServiceWorkerError::ErrorTypeAbort,
        base::ASCIIToUTF16(kShutdownErrorMessage)));
    return;
  }

  ServiceWorkerProviderHost* provider_host = GetContext()->GetProviderHost(
      render_process_id_, provider_id);
  if (!provider_host) {
    BadMessageReceived();
    return;
  }
  if (!provider_host->IsContextAlive()) {
    Send(new ServiceWorkerMsg_ServiceWorkerRegistrationError(
        thread_id,
        request_id,
        blink::WebServiceWorkerError::ErrorTypeAbort,
        base::ASCIIToUTF16(kShutdownErrorMessage)));
    return;
  }

  if (!CanUnregisterServiceWorker(provider_host->document_url(), pattern)) {
    BadMessageReceived();
    return;
  }

  GetContext()->UnregisterServiceWorker(
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
  if (!GetContext())
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
  if (!GetContext())
    return;
  if (GetContext()->GetProviderHost(render_process_id_, provider_id)) {
    BadMessageReceived();
    return;
  }
  scoped_ptr<ServiceWorkerProviderHost> provider_host(
      new ServiceWorkerProviderHost(
          render_process_id_, provider_id, GetContext()->AsWeakPtr(), this));
  GetContext()->AddProviderHost(provider_host.Pass());
}

void ServiceWorkerDispatcherHost::OnProviderDestroyed(int provider_id) {
  if (!GetContext())
    return;
  if (!GetContext()->GetProviderHost(render_process_id_, provider_id)) {
    BadMessageReceived();
    return;
  }
  GetContext()->RemoveProviderHost(render_process_id_, provider_id);
}

void ServiceWorkerDispatcherHost::OnSetHostedVersionId(
    int provider_id, int64 version_id) {
  if (!GetContext())
    return;
  ServiceWorkerProviderHost* provider_host =
      GetContext()->GetProviderHost(render_process_id_, provider_id);
  if (!provider_host) {
    BadMessageReceived();
    return;
  }
  if (!provider_host->IsContextAlive())
    return;
  if (!provider_host->SetHostedVersionId(version_id))
    BadMessageReceived();
}

ServiceWorkerHandle* ServiceWorkerDispatcherHost::FindHandle(int thread_id,
                                                             int64 version_id) {
  for (IDMap<ServiceWorkerHandle, IDMapOwnPointer>::iterator iter(&handles_);
       !iter.IsAtEnd();
       iter.Advance()) {
    ServiceWorkerHandle* handle = iter.GetCurrentValue();
    DCHECK(handle);
    if (handle->thread_id() == thread_id && handle->version() &&
        handle->version()->version_id() == version_id)
      return handle;
  }
  return NULL;
}

void ServiceWorkerDispatcherHost::RegistrationComplete(
    int thread_id,
    int request_id,
    ServiceWorkerStatusCode status,
    int64 registration_id,
    int64 version_id) {
  if (!GetContext())
    return;

  if (status != SERVICE_WORKER_OK) {
    SendRegistrationError(thread_id, request_id, status);
    return;
  }

  ServiceWorkerVersion* version = GetContext()->GetLiveVersion(version_id);
  DCHECK(version);
  DCHECK_EQ(registration_id, version->registration_id());
  ServiceWorkerObjectInfo info;
  ServiceWorkerHandle* handle = FindHandle(thread_id, version_id);
  if (handle) {
    info = handle->GetObjectInfo();
    handle->IncrementRefCount();
  } else {
    scoped_ptr<ServiceWorkerHandle> new_handle = ServiceWorkerHandle::Create(
        GetContext()->AsWeakPtr(), this, thread_id, version);
    info = new_handle->GetObjectInfo();
    RegisterServiceWorkerHandle(new_handle.Pass());
  }
  Send(new ServiceWorkerMsg_ServiceWorkerRegistered(
      thread_id, request_id, info));
}

void ServiceWorkerDispatcherHost::OnWorkerScriptLoaded(int embedded_worker_id) {
  if (!GetContext())
    return;
  EmbeddedWorkerRegistry* registry = GetContext()->embedded_worker_registry();
  if (!registry->CanHandle(embedded_worker_id))
    return;
  registry->OnWorkerScriptLoaded(render_process_id_, embedded_worker_id);
}

void ServiceWorkerDispatcherHost::OnWorkerScriptLoadFailed(
    int embedded_worker_id) {
  if (!GetContext())
    return;
  EmbeddedWorkerRegistry* registry = GetContext()->embedded_worker_registry();
  if (!registry->CanHandle(embedded_worker_id))
    return;
  registry->OnWorkerScriptLoadFailed(render_process_id_, embedded_worker_id);
}

void ServiceWorkerDispatcherHost::OnWorkerStarted(
    int thread_id, int embedded_worker_id) {
  if (!GetContext())
    return;
  EmbeddedWorkerRegistry* registry = GetContext()->embedded_worker_registry();
  if (!registry->CanHandle(embedded_worker_id))
    return;
  registry->OnWorkerStarted(render_process_id_, thread_id, embedded_worker_id);
}

void ServiceWorkerDispatcherHost::OnWorkerStopped(int embedded_worker_id) {
  if (!GetContext())
    return;
  EmbeddedWorkerRegistry* registry = GetContext()->embedded_worker_registry();
  if (!registry->CanHandle(embedded_worker_id))
    return;
  registry->OnWorkerStopped(render_process_id_, embedded_worker_id);
}

void ServiceWorkerDispatcherHost::OnPausedAfterDownload(
    int embedded_worker_id) {
  if (!GetContext())
    return;
  GetContext()->embedded_worker_registry()->OnPausedAfterDownload(
      render_process_id_, embedded_worker_id);
}

void ServiceWorkerDispatcherHost::OnReportException(
    int embedded_worker_id,
    const base::string16& error_message,
    int line_number,
    int column_number,
    const GURL& source_url) {
  if (!GetContext())
    return;
  EmbeddedWorkerRegistry* registry = GetContext()->embedded_worker_registry();
  if (!registry->CanHandle(embedded_worker_id))
    return;
  registry->OnReportException(embedded_worker_id,
                              error_message,
                              line_number,
                              column_number,
                              source_url);
}

void ServiceWorkerDispatcherHost::OnReportConsoleMessage(
    int embedded_worker_id,
    const EmbeddedWorkerHostMsg_ReportConsoleMessage_Params& params) {
  if (!GetContext())
    return;
  EmbeddedWorkerRegistry* registry = GetContext()->embedded_worker_registry();
  if (!registry->CanHandle(embedded_worker_id))
    return;
  registry->OnReportConsoleMessage(embedded_worker_id,
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

ServiceWorkerContextCore* ServiceWorkerDispatcherHost::GetContext() {
  return context_wrapper_->context();
}

}  // namespace content
