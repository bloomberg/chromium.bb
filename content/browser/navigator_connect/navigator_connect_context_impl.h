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

namespace content {

class MessagePortMessageFilter;
class NavigatorConnectService;
class NavigatorConnectServiceFactory;
struct NavigatorConnectClient;
struct TransferredMessagePort;

// Tracks all active navigator.connect connections, as well as available service
// factories. Delegates connection requests to the correct factory and passes
// messages on to the correct service.
// One instance of this class exists per StoragePartition.
// TODO(mek): Somehow clean up connections when the client side goes away.
class NavigatorConnectContextImpl : public NavigatorConnectContext {
 public:
  using ConnectCallback =
      base::Callback<void(const TransferredMessagePort& message_port,
                          int message_port_route_id,
                          bool success)>;

  explicit NavigatorConnectContextImpl();

  // Called when a new connection request comes in from a client. Finds the
  // correct service factory and passes the connection request off to there.
  // Can call the callback before this method call returns.
  void Connect(NavigatorConnectClient client,
               MessagePortMessageFilter* message_port_message_filter,
               const ConnectCallback& callback);

  // NavigatorConnectContext implementation.
  void AddFactory(scoped_ptr<NavigatorConnectServiceFactory> factory) override;

 private:
  ~NavigatorConnectContextImpl() override;

  void AddFactoryOnIOThread(scoped_ptr<NavigatorConnectServiceFactory> factory);

  // Callback called by service factories when a connection succeeded or failed.
  void OnConnectResult(const NavigatorConnectClient& client,
                       int client_message_port_id,
                       int client_port_route_id,
                       const ConnectCallback& callback,
                       MessagePortDelegate* delegate,
                       bool data_as_values);

  // List of factories to try to handle URLs.
  ScopedVector<NavigatorConnectServiceFactory> service_factories_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_NAVIGATOR_CONNECT_NAVIGATOR_CONNECT_CONTEXT_IMPL_H_
