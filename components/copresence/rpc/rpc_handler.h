// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_RPC_RPC_HANDLER_H_
#define COMPONENTS_COPRESENCE_RPC_RPC_HANDLER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/copresence/public/copresence_client_delegate.h"

namespace copresence {

class ReportRequest;
class WhispernetClient;

// This class currently handles all communication with the copresence server.
// NOTE: This class is a stub.
class RpcHandler {
 public:
  // A callback to indicate whether handler initialization succeeded.
  typedef base::Callback<void(bool)> SuccessCallback;

  // Constructor. May call the server to register a device,
  // so completion status is indicated by the SuccessCallback.
  RpcHandler(CopresenceClientDelegate* delegate,
             SuccessCallback init_done_callback);

  virtual ~RpcHandler();

  // Send a report request
  void SendReportRequest(scoped_ptr<copresence::ReportRequest> request);
  void SendReportRequest(scoped_ptr<copresence::ReportRequest> request,
                         const std::string& app_id,
                         const StatusCallback& callback);

  // Report a set of tokens to the server for a given medium.
  void ReportTokens(copresence::TokenMedium medium,
                    const std::vector<std::string>& tokens);

  // Create the directive handler and connect it to the whispernet client.
  void ConnectToWhispernet(WhispernetClient* whispernet_client);

  // Disconnect the directive handler from the whispernet client.
  void DisconnectFromWhispernet();

 private:
  DISALLOW_COPY_AND_ASSIGN(RpcHandler);
};

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_RPC_RPC_HANDLER_H_
