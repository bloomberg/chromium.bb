// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NAVIGATOR_CONNECT_NAVIGATOR_CONNECT_CONTEXT_IMPL_H_
#define CONTENT_BROWSER_NAVIGATOR_CONNECT_NAVIGATOR_CONNECT_CONTEXT_IMPL_H_

#include <map>
#include "base/callback_forward.h"
#include "base/memory/scoped_vector.h"
#include "content/public/browser/message_port_delegate.h"
#include "content/public/browser/navigator_connect_context.h"

// Windows headers will redefine SendMessage.
#ifdef SendMessage
#undef SendMessage
#endif

class GURL;

namespace content {

class MessagePortMessageFilter;
class NavigatorConnectService;
class NavigatorConnectServiceFactory;
struct NavigatorConnectClient;
class ServicePortServiceImpl;
struct TransferredMessagePort;

// Tracks all active navigator.services connections, as well as available
// service factories. Delegates connection requests to the correct factory and
// passes messages on to the correct service.
// One instance of this class exists per StoragePartition.
// TODO(mek): Clean up connections, fire of closed events when connections die.
// TODO(mek): Update service side API to be ServicePort based and get rid of
// MessagePort dependency.
// TODO(mek): Make ServicePorts that live in a service worker be able to survive
// the worker being restarted.
class NavigatorConnectContextImpl : public NavigatorConnectContext,
                                    public MessagePortDelegate {
 public:
  using ConnectCallback =
      base::Callback<void(int message_port_id, bool success)>;

  explicit NavigatorConnectContextImpl();

  // Called when a new connection request comes in from a client. Finds the
  // correct service factory and passes the connection request off to there.
  // Can call the callback before this method call returns.
  void Connect(const GURL& target_url,
               const GURL& origin,
               ServicePortServiceImpl* service_port_service,
               const ConnectCallback& callback);

  // Called by a ServicePortServiceImpl instance when it is about to be
  // destroyed to inform this class that all its connections are no longer
  // valid.
  void ServicePortServiceDestroyed(
      ServicePortServiceImpl* service_port_service);

  // NavigatorConnectContext implementation.
  void AddFactory(scoped_ptr<NavigatorConnectServiceFactory> factory) override;

  // MessagePortDelegate implementation.
  void SendMessage(
      int route_id,
      const MessagePortMessage& message,
      const std::vector<TransferredMessagePort>& sent_message_ports) override;
  void SendMessagesAreQueued(int route_id) override;

 private:
  ~NavigatorConnectContextImpl() override;

  void AddFactoryOnIOThread(scoped_ptr<NavigatorConnectServiceFactory> factory);

  // Callback called by service factories when a connection succeeded or failed.
  void OnConnectResult(const NavigatorConnectClient& client,
                       int client_message_port_id,
                       const ConnectCallback& callback,
                       MessagePortDelegate* delegate,
                       bool data_as_values);

  // List of factories to try to handle URLs.
  ScopedVector<NavigatorConnectServiceFactory> service_factories_;

  // List of currently active ServicePorts.
  struct Port;
  std::map<int, Port> ports_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_NAVIGATOR_CONNECT_NAVIGATOR_CONNECT_CONTEXT_IMPL_H_
