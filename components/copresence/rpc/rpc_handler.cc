// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence/rpc/rpc_handler.h"

#include "base/bind.h"
#include "components/copresence/proto/data.pb.h"
#include "components/copresence/proto/rpcs.pb.h"
#include "components/copresence/public/copresence_client_delegate.h"
#include "components/copresence/public/whispernet_client.h"

namespace copresence {

RpcHandler::RpcHandler(CopresenceClientDelegate* delegate,
                       SuccessCallback init_done_callback) {
}

RpcHandler::~RpcHandler() {
}

void RpcHandler::SendReportRequest(
    scoped_ptr<copresence::ReportRequest> request) {
}

void RpcHandler::SendReportRequest(
    scoped_ptr<copresence::ReportRequest> request,
    const std::string& app_id,
    const StatusCallback& status_callback) {
}

void RpcHandler::ReportTokens(copresence::TokenMedium medium,
                              const std::vector<std::string>& tokens) {
}

void RpcHandler::ConnectToWhispernet(WhispernetClient* whispernet_client) {
}

void RpcHandler::DisconnectFromWhispernet() {
}

}  // namespace copresence
