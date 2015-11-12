// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_ENGINE_CONNECTION_MANAGER_H_
#define BLIMP_NET_ENGINE_CONNECTION_MANAGER_H_

#include "base/macros.h"
#include "blimp/net/blimp_connection.h"
#include "blimp/net/connection_handler.h"

namespace blimp {

// Coordinates the channel creation and authentication workflows for
// incoming (Engine) network connections.
//
// TODO(kmarshall): Add rate limiting and abuse handling logic.
class EngineConnectionManager : public ConnectionHandler,
                                public BlimpConnection::DisconnectObserver {
 public:
  // Caller is responsible for ensuring that |connection_handler| outlives
  // |this|.
  explicit EngineConnectionManager(ConnectionHandler* connection_handler);

  ~EngineConnectionManager() override;

  // Accepts new BlimpConnections from |transport_| as fast as they arrive.
  void StartListening();

  // ConnectionHandler implementation.
  // Handles a new connection, authenticates it, and passes it on to the
  // underlying ConnectionHandler.
  void HandleConnection(scoped_ptr<BlimpConnection> connection) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(EngineConnectionManager);
};

}  // namespace blimp

#endif  // BLIMP_NET_ENGINE_CONNECTION_MANAGER_H_
