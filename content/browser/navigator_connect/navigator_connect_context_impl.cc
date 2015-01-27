// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/navigator_connect/navigator_connect_context_impl.h"

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
  service_factories_.push_back(factory.release());
}

void NavigatorConnectContextImpl::Connect(const NavigatorConnectClient& client,
                                          const ConnectCallback& callback) {
  // Hold messages for port while setting up connection.
  MessagePortService::GetInstance()->HoldMessages(client.message_port_id);

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
    // Close port since connection failed.
    MessagePortService::GetInstance()->ClosePort(client.message_port_id);
    callback.Run(false);
    return;
  }

  // Actually initiate connection.
  factory->Connect(
      client, base::Bind(&NavigatorConnectContextImpl::OnConnectResult, this,
                         client, callback));
}

void NavigatorConnectContextImpl::OnConnectResult(
    const NavigatorConnectClient& client,
    const ConnectCallback& callback,
    MessagePortDelegate* delegate) {
  if (delegate) {
    MessagePortService::GetInstance()->UpdateMessagePort(
        client.message_port_id, delegate, client.message_port_id);
    MessagePortService::GetInstance()->ReleaseMessages(client.message_port_id);
    callback.Run(true);
  } else {
    // Close port since connection failed.
    MessagePortService::GetInstance()->ClosePort(client.message_port_id);
    callback.Run(false);
  }
}

}  // namespace content
