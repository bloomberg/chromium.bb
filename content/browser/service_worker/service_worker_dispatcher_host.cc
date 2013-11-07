// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_dispatcher_host.h"

#include "base/strings/utf_string_conversions.h"
#include "content/browser/service_worker/service_worker_context.h"
#include "content/common/service_worker_messages.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerError.h"
#include "url/gurl.h"

using WebKit::WebServiceWorkerError;

namespace content {

ServiceWorkerDispatcherHost::ServiceWorkerDispatcherHost(
    int render_process_id,
    ServiceWorkerContext* context) : context_(context) {}

ServiceWorkerDispatcherHost::~ServiceWorkerDispatcherHost() {}

namespace {
const char kDomainMismatchErrorMessage[] =
    "Scope and scripts do not have the same origin";
}

bool ServiceWorkerDispatcherHost::OnMessageReceived(const IPC::Message& message,
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
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

// TODO(alecflett): Store the service_worker_id keyed by (domain+pattern,
// script) so we don't always return a new service worker id.
static int64 NextWorkerId() {
  static int64 service_worker_id = 0;
  return service_worker_id++;
}

void ServiceWorkerDispatcherHost::OnRegisterServiceWorker(
    int32 thread_id,
    int32 request_id,
    const GURL& scope,
    const GURL& script_url) {
  if (!context_->IsEnabled()) {
    Send(new ServiceWorkerMsg_ServiceWorkerRegistrationError(
        thread_id,
        request_id,
        WebKit::WebServiceWorkerError::DisabledError,
        ASCIIToUTF16("ServiceWorker is disabled")));
    return;
  }

  // TODO(alecflett): This check is insufficient for release. Add a
  // ServiceWorker-specific policy query in
  // ChildProcessSecurityImpl. See http://crbug.com/311631.
  if (scope.GetOrigin() != script_url.GetOrigin()) {
    Send(new ServiceWorkerMsg_ServiceWorkerRegistrationError(
        thread_id,
        request_id,
        WebKit::WebServiceWorkerError::SecurityError,
        ASCIIToUTF16(kDomainMismatchErrorMessage)));
    return;
  }

  Send(new ServiceWorkerMsg_ServiceWorkerRegistered(
      thread_id, request_id, NextWorkerId()));
}

void ServiceWorkerDispatcherHost::OnUnregisterServiceWorker(int32 thread_id,
                                                            int32 request_id,
                                                            const GURL& scope) {
  // TODO(alecflett): This check is insufficient for release. Add a
  // ServiceWorker-specific policy query in
  // ChildProcessSecurityImpl. See http://crbug.com/311631.
  if (!context_->IsEnabled()) {
    Send(new ServiceWorkerMsg_ServiceWorkerRegistrationError(
        thread_id,
        request_id,
        WebServiceWorkerError::DisabledError,
        ASCIIToUTF16("ServiceWorker is disabled")));
    return;
  }

  Send(new ServiceWorkerMsg_ServiceWorkerUnregistered(thread_id, request_id));
}

}  // namespace content
