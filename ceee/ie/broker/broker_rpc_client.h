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
  virtual ~BrokerRpcClient();

  // Initialize connection with server.
  virtual HRESULT Connect(bool start_server);

  // Relese connection with server
  virtual void Disconnect();

  // Returns true if object ready for remote calls.
  virtual bool is_connected() const {
    return context_ != NULL && binding_handle_ != NULL;
  }

  // @name Remote calls.
  // @{
  // Calls FireEvent on server side.
  virtual HRESULT FireEvent(const char* event_name, const char* event_args);
  // Adds UMA to histograms on the server side. Either performance timings or
  // generic histogram.
  virtual HRESULT SendUmaHistogramTimes(const char* name, int sample);
  virtual HRESULT SendUmaHistogramData(const char* name,
                                       int sample,
                                       int min,
                                       int max,
                                       int bucket_count);
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
