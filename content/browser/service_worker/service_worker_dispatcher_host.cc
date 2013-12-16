// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_dispatcher_host.h"

#include "base/strings/utf_string_conversions.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/common/service_worker_messages.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerError.h"
#include "url/gurl.h"

using blink::WebServiceWorkerError;

namespace {

const char kDisabledErrorMessage[] =
    "ServiceWorker is disabled";
const char kDomainMismatchErrorMessage[] =
    "Scope and scripts do not have the same origin";

}  // namespace

namespace content {

ServiceWorkerDispatcherHost::ServiceWorkerDispatcherHost(
    int render_process_id)
    : render_process_id_(render_process_id) {
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

void ServiceWorkerDispatcherHost::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

bool ServiceWorkerDispatcherHost::OnMessageReceived(
    const IPC::Message& message,
    bool* message_was_ok) {
  if (IPC_MESSAGE_CLASS(message) != ServiceWorkerMsgStart)
    return false;

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
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void ServiceWorkerDispatcherHost::OnRegisterServiceWorker(
    int32 thread_id,
    int32 request_id,
    const GURL& pattern,
    const GURL& script_url) {
  if (!context_ || !context_->IsEnabled()) {
    Send(new ServiceWorkerMsg_ServiceWorkerRegistrationError(
        thread_id,
        request_id,
        WebServiceWorkerError::DisabledError,
        ASCIIToUTF16(kDisabledErrorMessage)));
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
        ASCIIToUTF16(kDomainMismatchErrorMessage)));
    return;
  }

  context_->RegisterServiceWorker(
      pattern,
      script_url,
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
  if (!context_ || !context_->IsEnabled()) {
    Send(new ServiceWorkerMsg_ServiceWorkerRegistrationError(
        thread_id,
        request_id,
        blink::WebServiceWorkerError::DisabledError,
        ASCIIToUTF16(kDisabledErrorMessage)));
    return;
  }

  context_->UnregisterServiceWorker(
      pattern,
      base::Bind(&ServiceWorkerDispatcherHost::UnregistrationComplete,
                 this,
                 thread_id,
                 request_id));
}

void ServiceWorkerDispatcherHost::OnProviderCreated(int provider_id) {
  if (!context_)
    return;
  if (context_->GetProviderHost(render_process_id_, provider_id)) {
    BadMessageReceived();
    return;
  }
  scoped_ptr<ServiceWorkerProviderHost> provider_host(
       new ServiceWorkerProviderHost(render_process_id_, provider_id));
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

void ServiceWorkerDispatcherHost::RegistrationComplete(
    int32 thread_id,
    int32 request_id,
    ServiceWorkerRegistrationStatus status,
    int64 registration_id) {
  if (status != REGISTRATION_OK) {
    SendRegistrationError(thread_id, request_id, status);
    return;
  }

  Send(new ServiceWorkerMsg_ServiceWorkerRegistered(
      thread_id, request_id, registration_id));
}

void ServiceWorkerDispatcherHost::UnregistrationComplete(
    int32 thread_id,
    int32 request_id,
    ServiceWorkerRegistrationStatus status) {
  if (status != REGISTRATION_OK) {
    SendRegistrationError(thread_id, request_id, status);
    return;
  }

  Send(new ServiceWorkerMsg_ServiceWorkerUnregistered(thread_id, request_id));
}

void ServiceWorkerDispatcherHost::SendRegistrationError(
    int32 thread_id,
    int32 request_id,
    ServiceWorkerRegistrationStatus status) {
  base::string16 error_message;
  blink::WebServiceWorkerError::ErrorType error_type;
  GetServiceWorkerRegistrationStatusResponse(
      status, &error_type, &error_message);
  Send(new ServiceWorkerMsg_ServiceWorkerRegistrationError(
      thread_id, request_id, error_type, error_message));
}

}  // namespace content
