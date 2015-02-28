// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NAVIGATOR_CONNECT_NAVIGATOR_CONNECT_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_NAVIGATOR_CONNECT_NAVIGATOR_CONNECT_DISPATCHER_HOST_H_

#include "content/public/browser/browser_message_filter.h"

class GURL;

namespace content {

class MessagePortMessageFilter;
class NavigatorConnectContextImpl;
struct NavigatorConnectClient;
struct TransferredMessagePort;

// Receives navigator.connect connection attempts from a child process.
// Attempts to find a service that serves the URL the connection is made to
// and sets up the actual connection.
// Constructed on the UI thread, but all other methods are called on the IO
// thread. Typically there is one instance of this class for each renderer
// process, and this class lives at least as long as the renderer process is
// alive (since this class is refcounted it could outlive the renderer process
// if it is still handling a connection attempt).
class NavigatorConnectDispatcherHost : public BrowserMessageFilter {
 public:
  NavigatorConnectDispatcherHost(
      const scoped_refptr<NavigatorConnectContextImpl>&
          navigator_connect_context,
      MessagePortMessageFilter* message_port_message_filter);

 private:
  ~NavigatorConnectDispatcherHost() override;

  // BrowserMessageFilter implementation.
  bool OnMessageReceived(const IPC::Message& message) override;

  // IPC Message handlers.
  void OnConnect(int thread_id,
                 int request_id,
                 const NavigatorConnectClient& client);

  // Callback called when the service worker finished handling the cross origin
  // connection event.
  void OnConnectResult(int thread_id,
                       int request_id,
                       const TransferredMessagePort& message_port,
                       int message_port_route_id,
                       bool accept_connection);

  scoped_refptr<NavigatorConnectContextImpl> navigator_connect_context_;
  MessagePortMessageFilter* const message_port_message_filter_;

  DISALLOW_COPY_AND_ASSIGN(NavigatorConnectDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_NAVIGATOR_CONNECT_NAVIGATOR_CONNECT_DISPATCHER_HOST_H_
