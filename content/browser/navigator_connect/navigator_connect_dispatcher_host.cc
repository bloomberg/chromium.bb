// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/navigator_connect/navigator_connect_dispatcher_host.h"

#include "content/browser/message_port_service.h"
#include "content/browser/navigator_connect/navigator_connect_context.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/navigator_connect_messages.h"
#include "content/common/navigator_connect_types.h"

namespace content {

NavigatorConnectDispatcherHost::NavigatorConnectDispatcherHost(
    const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context,
    const scoped_refptr<NavigatorConnectContext>& navigator_connect_context)
    : BrowserMessageFilter(NavigatorConnectMsgStart),
      service_worker_context_(service_worker_context),
      navigator_connect_context_(navigator_connect_context) {
}

NavigatorConnectDispatcherHost::~NavigatorConnectDispatcherHost() {
}

bool NavigatorConnectDispatcherHost::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(NavigatorConnectDispatcherHost, message)
    IPC_MESSAGE_HANDLER(NavigatorConnectHostMsg_Connect, OnConnect)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void NavigatorConnectDispatcherHost::OnConnect(
    int thread_id,
    int request_id,
    const CrossOriginServiceWorkerClient& client) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Hold messages for port while setting up connection.
  MessagePortService::GetInstance()->HoldMessages(client.message_port_id);

  // Find the right service worker to service this connection.
  service_worker_context_->context()->storage()->FindRegistrationForDocument(
      client.target_url,
      base::Bind(&NavigatorConnectDispatcherHost::GotServiceWorkerRegistration,
                 this, thread_id, request_id, client));
}

void NavigatorConnectDispatcherHost::GotServiceWorkerRegistration(
    int thread_id,
    int request_id,
    const CrossOriginServiceWorkerClient& client,
    ServiceWorkerStatusCode status,
    const scoped_refptr<ServiceWorkerRegistration>& registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (status != SERVICE_WORKER_OK) {
    // No service worker found, reject connection attempt.
    OnConnectResult(thread_id, request_id, client, registration, status, false);
    return;
  }

  ServiceWorkerVersion* active_version = registration->active_version();
  if (!active_version) {
    // No active version, reject connection attempt.
    OnConnectResult(thread_id, request_id, client, registration, status, false);
    return;
  }

  active_version->DispatchCrossOriginConnectEvent(
      base::Bind(&NavigatorConnectDispatcherHost::OnConnectResult, this,
                 thread_id, request_id, client, registration),
      client);
}

void NavigatorConnectDispatcherHost::OnConnectResult(
    int thread_id,
    int request_id,
    const CrossOriginServiceWorkerClient& client,
    const scoped_refptr<ServiceWorkerRegistration>& registration,
    ServiceWorkerStatusCode status,
    bool accept_connection) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (status != SERVICE_WORKER_OK || !accept_connection) {
    // Close port since connection failed.
    MessagePortService::GetInstance()->ClosePort(client.message_port_id);
    Send(new NavigatorConnectMsg_ConnectResult(thread_id, request_id, false));
    return;
  }

  // Register connection and post back result.
  navigator_connect_context_->RegisterConnection(client, registration);
  Send(new NavigatorConnectMsg_ConnectResult(thread_id, request_id, true));
}

}  // namespace content
