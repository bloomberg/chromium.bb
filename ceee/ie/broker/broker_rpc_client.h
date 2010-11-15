// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Broker RPC Client.

#ifndef CEEE_IE_BROKER_BROKER_RPC_CLIENT_H_
#define CEEE_IE_BROKER_BROKER_RPC_CLIENT_H_

#include <wtypes.h>
#include "base/basictypes.h"

// Class provides comunication with BrokerRpcServer.
class BrokerRpcClient {
 public:
  BrokerRpcClient();
  ~BrokerRpcClient();

  // Initialize connection with server.
  bool Connect();

  // Relese connection with server
  void Disconnect();

  // Returns true if object ready for remote calls.
  bool is_connected() const {
    return context_ != NULL && binding_handle_ != NULL;
  }

  // @name Remote calls.
  // @{
  // Calls FireEvent on server side.
  bool FireEvent(BSTR event_name, BSTR event_args);
  // @}

 private:
  void LockContext();
  void ReleaseContext();

  RPC_BINDING_HANDLE binding_handle_;
  // Context handle. It is required to make RPC server know number of active
  // clients.
  void* context_;
  DISALLOW_COPY_AND_ASSIGN(BrokerRpcClient);
};

#endif  // CEEE_IE_BROKER_BROKER_RPC_CLIENT_H_
