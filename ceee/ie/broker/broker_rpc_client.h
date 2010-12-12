// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Broker RPC Client.

#ifndef CEEE_IE_BROKER_BROKER_RPC_CLIENT_H_
#define CEEE_IE_BROKER_BROKER_RPC_CLIENT_H_

#include <wtypes.h>
#include "base/basictypes.h"

struct ICeeeBrokerRegistrar;

// Interface for sending events.
class IEventSender {
 public:
  virtual ~IEventSender() {}
  virtual HRESULT FireEvent(const char* event_name,
                            const char* event_args) = 0;
};

// Class provides communication with BrokerRpcServer.
class BrokerRpcClient : public IEventSender {
 public:
  // @param allow_restarts if true client will restart server if it is somehow
  //     gone after successful connecting. Client restarts server one time after
  //     each successful connecting.
  explicit BrokerRpcClient(bool allow_restarts);
  virtual ~BrokerRpcClient();

  // Initialize connection with server.
  // @param start_server if true method will try to start server if it is not
  //     started yet. Usually only tests pass false here.
  virtual HRESULT Connect(bool start_server);

  // Releases connection with server
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

 protected:
  // Starts ceee broker process. This is unittest seam.
  virtual HRESULT StartServer(ICeeeBrokerRegistrar** server);

 private:
  void LockContext();
  void ReleaseContext();

  template<class Function, class Params>
  HRESULT RunRpc(bool allow_restart,
                 Function rpc_function,
                 const Params& params);

  RPC_BINDING_HANDLE binding_handle_;
  // Context handle. It is required to make RPC server know number of active
  // clients.
  void* context_;
  bool allow_restarts_;
  DISALLOW_COPY_AND_ASSIGN(BrokerRpcClient);
};

HRESULT StartCeeeBroker(ICeeeBrokerRegistrar** broker);

#endif  // CEEE_IE_BROKER_BROKER_RPC_CLIENT_H_
