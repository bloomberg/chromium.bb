// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_script_context.h"

#include "base/logging.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/renderer/service_worker/embedded_worker_context_client.h"
#include "ipc/ipc_message.h"
#include "third_party/WebKit/public/web/WebServiceWorkerContextProxy.h"

namespace content {

ServiceWorkerScriptContext::ServiceWorkerScriptContext(
    EmbeddedWorkerContextClient* embedded_context,
    blink::WebServiceWorkerContextProxy* proxy)
    : embedded_context_(embedded_context),
      proxy_(proxy),
      current_request_id_(kInvalidRequestId) {
}

ServiceWorkerScriptContext::~ServiceWorkerScriptContext() {}

void ServiceWorkerScriptContext::OnMessageReceived(
    int request_id,
    const IPC::Message& message) {
  DCHECK_EQ(kInvalidRequestId, current_request_id_);
  current_request_id_ = request_id;
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ServiceWorkerScriptContext, message)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_InstallEvent, OnInstallEvent)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_FetchEvent, OnFetchEvent)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled);
  current_request_id_ = kInvalidRequestId;
}

void ServiceWorkerScriptContext::DidHandleInstallEvent(int request_id) {
  Send(request_id, ServiceWorkerHostMsg_InstallEventFinished());
}

void ServiceWorkerScriptContext::Send(int request_id,
                                      const IPC::Message& message) {
  embedded_context_->SendMessageToBrowser(request_id, message);
}

void ServiceWorkerScriptContext::OnInstallEvent(
    int active_version_embedded_worker_id) {
  proxy_->dispatchInstallEvent(current_request_id_);
}

void ServiceWorkerScriptContext::OnFetchEvent(
    const ServiceWorkerFetchRequest& request) {
  NOTIMPLEMENTED();
}

}  // namespace content
