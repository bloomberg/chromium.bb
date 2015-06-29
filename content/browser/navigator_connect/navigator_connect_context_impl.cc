// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/navigator_connect/navigator_connect_context_impl.h"

#include "content/browser/message_port_message_filter.h"
#include "content/browser/message_port_service.h"
#include "content/browser/navigator_connect/service_port_service_impl.h"
#include "content/public/browser/message_port_provider.h"
#include "content/public/browser/navigator_connect_service_factory.h"
#include "content/public/common/navigator_connect_client.h"

namespace content {

struct NavigatorConnectContextImpl::Port {
  int message_port_id;
  // Set to nullptr when the ServicePortService goes away.
  ServicePortServiceImpl* service;
};

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
    const GURL& target_url,
    const GURL& origin,
    ServicePortServiceImpl* service_port_service,
    const ConnectCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Create a new message channel. Use |this| as delegate for both ports until
  // the real delegate for the service port is known later on.
  int client_port_id;
  int service_port;
  MessagePortProvider::CreateMessageChannel(this, &client_port_id,
                                            &service_port);
  // Hold messages send to the client while setting up connection.
  MessagePortService::GetInstance()->HoldMessages(client_port_id);

  Port& client_port = ports_[client_port_id];
  client_port.message_port_id = client_port_id;
  client_port.service = service_port_service;

  // The message_port_id stored in the client object is the one associated with
  // the service.
  NavigatorConnectClient client(target_url, origin, service_port);

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
    OnConnectResult(client, client_port_id, callback, nullptr, false);
    return;
  }

  // Actually initiate connection.
  factory->Connect(
      client, base::Bind(&NavigatorConnectContextImpl::OnConnectResult, this,
                         client, client_port_id, callback));
}

void NavigatorConnectContextImpl::ServicePortServiceDestroyed(
    ServicePortServiceImpl* service_port_service) {
  for (auto& port : ports_) {
    if (port.second.service != service_port_service)
      continue;
    port.second.service = nullptr;
    // TODO(mek): Should actually inform other side of connections that the
    // connection was closed, or in the case of service workers somehow keep
    // track of the connection.
  }
}

void NavigatorConnectContextImpl::SendMessage(
    int route_id,
    const MessagePortMessage& message,
    const std::vector<TransferredMessagePort>& sent_message_ports) {
  DCHECK(ports_.find(route_id) != ports_.end());
  const Port& port = ports_[route_id];
  if (!port.service) {
    // TODO(mek): Figure out what to do in this situation.
    return;
  }

  port.service->PostMessageToClient(route_id, message, sent_message_ports);
}

void NavigatorConnectContextImpl::SendMessagesAreQueued(int route_id) {
  NOTREACHED() << "navigator.services endpoints should never queue messages.";
}

void NavigatorConnectContextImpl::OnConnectResult(
    const NavigatorConnectClient& client,
    int client_message_port_id,
    const ConnectCallback& callback,
    MessagePortDelegate* delegate,
    bool data_as_values) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (delegate) {
    DCHECK(!data_as_values) << "Data as values is not currently implemented";
    // TODO(mek): Might have to do something else if the client connection got
    // severed while the service side connection was being set up.

    // Update service side port with delegate.
    MessagePortService::GetInstance()->UpdateMessagePort(
        client.message_port_id, delegate, client.message_port_id);
    callback.Run(client_message_port_id, true);
    MessagePortService::GetInstance()->ReleaseMessages(client_message_port_id);
  } else {
    // Destroy ports since connection failed.
    MessagePortService::GetInstance()->Destroy(client.message_port_id);
    MessagePortService::GetInstance()->Destroy(client_message_port_id);
    ports_.erase(client_message_port_id);
    callback.Run(MSG_ROUTING_NONE, false);
  }
}

}  // namespace content
