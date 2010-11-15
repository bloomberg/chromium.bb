// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Broker RPC Server.

#ifndef CEEE_IE_BROKER_BROKER_RPC_SERVER_H_
#define CEEE_IE_BROKER_BROKER_RPC_SERVER_H_

#include <wtypes.h>
#include "base/basictypes.h"

// Serving requests from BrokerRpcClient.
class BrokerRpcServer {
 public:
  BrokerRpcServer();
  ~BrokerRpcServer();

  // Registers RPC interface and starts listening for requests.
  bool Start();

  // Unregister RPC interface.
  bool Stop();

  // Returns true if object is listening.
  bool is_started() const;

 private:
  // State of object.
  bool is_started_;
  DWORD current_thread_;

  DISALLOW_COPY_AND_ASSIGN(BrokerRpcServer);
};

#endif  // CEEE_IE_BROKER_BROKER_RPC_SERVER_H_
