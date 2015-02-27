// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/navigator_connect/navigator_connect_context_impl.h"

#include "content/browser/message_port_message_filter.h"
#include "content/browser/message_port_service.h"
#include "content/public/browser/navigator_connect_service_factory.h"
#include "content/public/common/navigator_connect_client.h"

namespace content {

NavigatorConnectContextImpl::NavigatorConnectContextImpl() {
}

NavigatorConnectContextImpl::~NavigatorConnectContextImpl() {
}

void NavigatorConnectContextImpl::AddFactory(
    scoped_ptr<NavigatorConnectServiceFactory> factory) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&NavigatorConnectContextImpl::AddFactoryOnIOThread, this,
                 base::Passed(&factory)));
}

void NavigatorConnectContextImpl::AddFactoryOnIOThread(
    scoped_ptr<NavigatorConnectServiceFactory> factory) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  service_factories_.push_back(factory.release());
}

void NavigatorConnectContextImpl::Connect(
    NavigatorConnectClient client,
    MessagePortMessageFilter* message_port_message_filter,
    const ConnectCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Create a new message channel. Client port is setup to talk to client
  // process, service port is initially setup without a delegate.
  MessagePortService* message_port_service = MessagePortService::GetInstance();
  int client_port;
  int client_port_route_id = message_port_message_filter->GetNextRoutingID();
  message_port_service->Create(client_port_route_id,
                               message_port_message_filter, &client_port);
  int service_port;
  message_port_service->Create(MSG_ROUTING_NONE, nullptr, &service_port);
  message_port_service->Entangle(client_port, service_port);
  message_port_service->Entangle(service_port, client_port);
  // Hold messages on client port while setting up connection.
  message_port_service->HoldMessages(client_port);

  // The message_port_id stored in the client object is the one associated with
  // the service.
  client.message_port_id = service_port;

  // Find factory to handle request, more recently added factories should take
  // priority as per comment at NavigatorConnectContext::AddFactory..
  NavigatorConnectServiceFactory* factory = nullptr;
  for (auto it = service_factories_.rbegin(); it != service_factories_.rend();
       ++it) {
    if ((*it)->HandlesUrl(client.target_url)) {
      factory = *it;
      break;
    }
  }

  if (!factory) {
    // No factories found.
    // Destroy ports since connection failed.
    message_port_service->Destroy(client_port);
    message_port_service->Destroy(service_port);
    callback.Run(MSG_ROUTING_NONE, MSG_ROUTING_NONE, false);
    return;
  }

  // Actually initiate connection.
  factory->Connect(
      client, base::Bind(&NavigatorConnectContextImpl::OnConnectResult, this,
                         client, client_port, client_port_route_id, callback));
}

void NavigatorConnectContextImpl::OnConnectResult(
    const NavigatorConnectClient& client,
    int client_message_port_id,
    int client_port_route_id,
    const ConnectCallback& callback,
    MessagePortDelegate* delegate) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (delegate) {
    // Update service side port with delegate.
    MessagePortService::GetInstance()->UpdateMessagePort(
        client.message_port_id, delegate, client.message_port_id);
    callback.Run(client_message_port_id, client_port_route_id, true);
  } else {
    // Destroy ports since connection failed.
    MessagePortService::GetInstance()->Destroy(client.message_port_id);
    MessagePortService::GetInstance()->Destroy(client_message_port_id);
    callback.Run(MSG_ROUTING_NONE, MSG_ROUTING_NONE, false);
  }
}

}  // namespace content
