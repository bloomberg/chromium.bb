// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_script_context.h"

#include "base/logging.h"
#include "content/child/webmessageportchannel_impl.h"
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
      current_request_id_(kInvalidServiceWorkerRequestId) {
}

ServiceWorkerScriptContext::~ServiceWorkerScriptContext() {}

void ServiceWorkerScriptContext::OnMessageReceived(
    int request_id,
    const IPC::Message& message) {
  DCHECK_EQ(kInvalidServiceWorkerRequestId, current_request_id_);
  current_request_id_ = request_id;
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ServiceWorkerScriptContext, message)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ActivateEvent, OnActivateEvent)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_FetchEvent, OnFetchEvent)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_InstallEvent, OnInstallEvent)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_Message, OnPostMessage)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_SyncEvent, OnSyncEvent)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled);
  current_request_id_ = kInvalidServiceWorkerRequestId;
}

void ServiceWorkerScriptContext::DidHandleActivateEvent(
    int request_id,
    blink::WebServiceWorkerEventResult result) {
  Reply(request_id, ServiceWorkerHostMsg_ActivateEventFinished(result));
}

void ServiceWorkerScriptContext::DidHandleInstallEvent(
    int request_id,
    blink::WebServiceWorkerEventResult result) {
  Reply(request_id, ServiceWorkerHostMsg_InstallEventFinished(result));
}

void ServiceWorkerScriptContext::DidHandleFetchEvent(
    int request_id,
    ServiceWorkerFetchEventResult result,
    const ServiceWorkerResponse& response) {
  Reply(request_id, ServiceWorkerHostMsg_FetchEventFinished(result, response));
}

void ServiceWorkerScriptContext::DidHandleSyncEvent(int request_id) {
  Reply(request_id, ServiceWorkerHostMsg_SyncEventFinished());
}

void ServiceWorkerScriptContext::Send(IPC::Message* message) {
  embedded_context_->Send(message);
}

void ServiceWorkerScriptContext::Reply(int request_id,
                                       const IPC::Message& message) {
  embedded_context_->SendReplyToBrowser(request_id, message);
}

void ServiceWorkerScriptContext::OnActivateEvent() {
  proxy_->dispatchActivateEvent(current_request_id_);
}

void ServiceWorkerScriptContext::OnInstallEvent(int active_version_id) {
  proxy_->dispatchInstallEvent(current_request_id_);
}

void ServiceWorkerScriptContext::OnFetchEvent(
    const ServiceWorkerFetchRequest& request) {
  // TODO(falken): Pass in the request.
  proxy_->dispatchFetchEvent(current_request_id_);
}

void ServiceWorkerScriptContext::OnPostMessage(
    const base::string16& message,
    const std::vector<int>& sent_message_port_ids,
    const std::vector<int>& new_routing_ids) {
  std::vector<WebMessagePortChannelImpl*> ports;
  if (!sent_message_port_ids.empty()) {
    base::MessageLoopProxy* loop_proxy = embedded_context_->main_thread_proxy();
    ports.resize(sent_message_port_ids.size());
    for (size_t i = 0; i < sent_message_port_ids.size(); ++i) {
      ports[i] = new WebMessagePortChannelImpl(
          new_routing_ids[i], sent_message_port_ids[i], loop_proxy);
    }
  }

  proxy_->dispatchMessageEvent(message, ports);
}

void ServiceWorkerScriptContext::OnSyncEvent() {
  proxy_->dispatchSyncEvent(current_request_id_);
}

}  // namespace content
