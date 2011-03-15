// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/worker_message_filter.h"

#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "content/browser/worker_host/message_port_service.h"
#include "content/browser/worker_host/worker_service.h"
#include "content/common/worker_messages.h"

WorkerMessageFilter::WorkerMessageFilter(
    int render_process_id,
    URLRequestContextGetter* request_context,
    ResourceDispatcherHost* resource_dispatcher_host,
    CallbackWithReturnValue<int>::Type* next_routing_id)
    : render_process_id_(render_process_id),
      request_context_(request_context),
      resource_dispatcher_host_(resource_dispatcher_host),
      next_routing_id_(next_routing_id) {
}

WorkerMessageFilter::~WorkerMessageFilter() {
}

void WorkerMessageFilter::OnChannelClosing() {
  BrowserMessageFilter::OnChannelClosing();

  MessagePortService::GetInstance()->OnWorkerMessageFilterClosing(this);
  WorkerService::GetInstance()->OnWorkerMessageFilterClosing(this);
}

bool WorkerMessageFilter::OnMessageReceived(const IPC::Message& message,
                                            bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(WorkerMessageFilter, message, *message_was_ok)
    // Worker messages.
    // Only sent from renderer for now, until we have nested workers.
    IPC_MESSAGE_HANDLER(ViewHostMsg_CreateWorker, OnCreateWorker)
    // Only sent from renderer for now, until we have nested workers.
    IPC_MESSAGE_HANDLER(ViewHostMsg_LookupSharedWorker, OnLookupSharedWorker)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CancelCreateDedicatedWorker,
                        OnCancelCreateDedicatedWorker)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ForwardToWorker, OnForwardToWorker)
    // Only sent from renderer.
    IPC_MESSAGE_HANDLER(ViewHostMsg_DocumentDetached, OnDocumentDetached)
    // Message Port related messages.
    IPC_MESSAGE_HANDLER(WorkerProcessHostMsg_CreateMessagePort,
                        OnCreateMessagePort)
    IPC_MESSAGE_FORWARD(WorkerProcessHostMsg_DestroyMessagePort,
                        MessagePortService::GetInstance(),
                        MessagePortService::Destroy)
    IPC_MESSAGE_FORWARD(WorkerProcessHostMsg_Entangle,
                        MessagePortService::GetInstance(),
                        MessagePortService::Entangle)
    IPC_MESSAGE_FORWARD(WorkerProcessHostMsg_PostMessage,
                        MessagePortService::GetInstance(),
                        MessagePortService::PostMessage)
    IPC_MESSAGE_FORWARD(WorkerProcessHostMsg_QueueMessages,
                        MessagePortService::GetInstance(),
                        MessagePortService::QueueMessages)
    IPC_MESSAGE_FORWARD(WorkerProcessHostMsg_SendQueuedMessages,
                        MessagePortService::GetInstance(),
                        MessagePortService::SendQueuedMessages)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()

  return handled;
}

int WorkerMessageFilter::GetNextRoutingID() {
  return next_routing_id_->Run();
}

void WorkerMessageFilter::OnCreateWorker(
    const ViewHostMsg_CreateWorker_Params& params,
    int* route_id) {
  *route_id = params.route_id != MSG_ROUTING_NONE ?
      params.route_id : next_routing_id_->Run();
  WorkerService::GetInstance()->CreateWorker(
      params, *route_id, this, request_context_);
}

void WorkerMessageFilter::OnLookupSharedWorker(
    const ViewHostMsg_CreateWorker_Params& params,
    bool* exists,
    int* route_id,
    bool* url_error) {
  *route_id = next_routing_id_->Run();

  bool incognito = static_cast<ChromeURLRequestContext*>(
      request_context_->GetURLRequestContext())->is_incognito();
  WorkerService::GetInstance()->LookupSharedWorker(
      params, *route_id, this, incognito, exists, url_error);
}

void WorkerMessageFilter::OnCancelCreateDedicatedWorker(int route_id) {
  WorkerService::GetInstance()->CancelCreateDedicatedWorker(route_id, this);
}

void WorkerMessageFilter::OnForwardToWorker(const IPC::Message& message) {
  WorkerService::GetInstance()->ForwardToWorker(message, this);
}

void WorkerMessageFilter::OnDocumentDetached(unsigned long long document_id) {
  WorkerService::GetInstance()->DocumentDetached(document_id, this);
}

void WorkerMessageFilter::OnCreateMessagePort(int *route_id,
                                              int* message_port_id) {
  *route_id = next_routing_id_->Run();
  MessagePortService::GetInstance()->Create(*route_id, this, message_port_id);
}
