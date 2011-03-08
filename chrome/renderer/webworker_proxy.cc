// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/webworker_proxy.h"

#include "chrome/common/render_messages.h"
#include "chrome/common/webmessageportchannel_impl.h"
#include "chrome/common/worker_messages.h"
#include "content/common/child_thread.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebWorkerClient.h"

using WebKit::WebCommonWorkerClient;
using WebKit::WebMessagePortChannel;
using WebKit::WebMessagePortChannelArray;
using WebKit::WebString;
using WebKit::WebURL;
using WebKit::WebWorkerClient;

WebWorkerProxy::WebWorkerProxy(
    WebWorkerClient* client,
    ChildThread* child_thread,
    int render_view_route_id,
    int parent_appcache_host_id)
    : WebWorkerBase(child_thread, 0, MSG_ROUTING_NONE, render_view_route_id,
                    parent_appcache_host_id),
      client_(client) {
  // TODO(atwilson): Change to pass in a real document_id when we support nested
  // workers.
}

WebWorkerProxy::~WebWorkerProxy() {
  // If we're midway through starting a worker, cancel it.
  CancelCreation();
}

void WebWorkerProxy::CancelCreation() {
  if (route_id_ == MSG_ROUTING_NONE)
    return;

  // Tell the browser to not start our queued worker.
  if (!IsStarted())
    child_thread_->Send(new ViewHostMsg_CancelCreateDedicatedWorker(route_id_));
}

void WebWorkerProxy::startWorkerContext(
    const WebURL& script_url,
    const WebString& user_agent,
    const WebString& source_code) {
  CreateDedicatedWorkerContext(script_url, user_agent, source_code);
}

void WebWorkerProxy::terminateWorkerContext() {
  if (route_id_ != MSG_ROUTING_NONE) {
    Send(new WorkerMsg_TerminateWorkerContext(route_id_));
    CancelCreation();
    Disconnect();
  }
}

void WebWorkerProxy::postMessageToWorkerContext(
    const WebString& message, const WebMessagePortChannelArray& channels) {
  std::vector<int> message_port_ids(channels.size());
  std::vector<int> routing_ids(channels.size());
  for (size_t i = 0; i < channels.size(); ++i) {
    WebMessagePortChannelImpl* webchannel =
        static_cast<WebMessagePortChannelImpl*>(channels[i]);
    message_port_ids[i] = webchannel->message_port_id();
    webchannel->QueueMessages();
    routing_ids[i] = MSG_ROUTING_NONE;
    DCHECK(message_port_ids[i] != MSG_ROUTING_NONE);
  }

  Send(new WorkerMsg_PostMessage(
      route_id_, message, message_port_ids, routing_ids));
}

void WebWorkerProxy::workerObjectDestroyed() {
  Send(new WorkerMsg_WorkerObjectDestroyed(route_id_));
  delete this;
}

void WebWorkerProxy::clientDestroyed() {
}

bool WebWorkerProxy::OnMessageReceived(const IPC::Message& message) {
  if (!client_)
    return false;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebWorkerProxy, message)
    IPC_MESSAGE_HANDLER(ViewMsg_WorkerCreated, OnWorkerCreated)
    IPC_MESSAGE_HANDLER(WorkerMsg_PostMessage, OnPostMessage)
    IPC_MESSAGE_FORWARD(WorkerHostMsg_PostExceptionToWorkerObject,
                        client_,
                        WebWorkerClient::postExceptionToWorkerObject)
    IPC_MESSAGE_HANDLER(WorkerHostMsg_PostConsoleMessageToWorkerObject,
                        OnPostConsoleMessageToWorkerObject)
    IPC_MESSAGE_FORWARD(WorkerHostMsg_ConfirmMessageFromWorkerObject,
                        client_,
                        WebWorkerClient::confirmMessageFromWorkerObject)
    IPC_MESSAGE_FORWARD(WorkerHostMsg_ReportPendingActivity,
                        client_,
                        WebWorkerClient::reportPendingActivity)
    IPC_MESSAGE_FORWARD(WorkerHostMsg_WorkerContextDestroyed,
                        static_cast<WebCommonWorkerClient*>(client_),
                        WebCommonWorkerClient::workerContextDestroyed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void WebWorkerProxy::OnWorkerCreated() {
  // The worker is created - now send off the CreateWorkerContext message and
  // any other queued messages
  SendQueuedMessages();
}

void WebWorkerProxy::OnPostMessage(
    const string16& message,
    const std::vector<int>& sent_message_port_ids,
    const std::vector<int>& new_routing_ids) {
  DCHECK(new_routing_ids.size() == sent_message_port_ids.size());
  WebMessagePortChannelArray channels(sent_message_port_ids.size());
  for (size_t i = 0; i < sent_message_port_ids.size(); ++i) {
    channels[i] = new WebMessagePortChannelImpl(
        new_routing_ids[i], sent_message_port_ids[i]);
  }

  client_->postMessageToWorkerObject(message, channels);
}

void WebWorkerProxy::OnPostConsoleMessageToWorkerObject(
      const WorkerHostMsg_PostConsoleMessageToWorkerObject_Params& params) {
  client_->postConsoleMessageToWorkerObject(params.source_identifier,
      params.message_type, params.message_level,
      params.message, params.line_number, params.source_url);
}

