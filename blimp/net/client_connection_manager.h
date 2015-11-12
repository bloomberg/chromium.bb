// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_CLIENT_CONNECTION_MANAGER_H_
#define BLIMP_NET_CLIENT_CONNECTION_MANAGER_H_

#include "base/macros.h"
#include "blimp/net/blimp_connection.h"
#include "blimp/net/connection_handler.h"

namespace blimp {

// Coordinates the channel creation and authentication workflows for
// outgoing (Client) network connections.
// Attempts to reconnect if an authenticated connection is
// disconnected.
class ClientConnectionManager : public ConnectionHandler,
                                BlimpConnection::DisconnectObserver {
  // Caller is responsible for ensuring that |client_browser_session|
  // outlives |this|.
  explicit ClientConnectionManager(ConnectionHandler* connection_handler);

  ~ClientConnectionManager() override;

  void Connect();

  // ConnectionHandler implementation.
  // Handles a new connection and authenticates it before passing it on to
  // the underlying ConnectionHandler.
  void HandleConnection(scoped_ptr<BlimpConnection> connection) override;

  // BlimpConnection::DisconnectObserver implementation.
  // Used to implement reconnection logic on unexpected disconnections.
  void OnDisconnected() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ClientConnectionManager);
};

}  // namespace blimp

#endif  // BLIMP_NET_CLIENT_CONNECTION_MANAGER_H_
