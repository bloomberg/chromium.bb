// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence/public/copresence_client.h"

#include "base/bind.h"
#include "components/copresence/public/copresence_client_delegate.h"
#include "components/copresence/public/whispernet_client.h"
#include "components/copresence/rpc/rpc_handler.h"

namespace copresence {

PendingRequest::PendingRequest(const copresence::ReportRequest& report,
                               const std::string app_id,
                               const StatusCallback& callback)
    : report(report), app_id(app_id), callback(callback) {
}

PendingRequest::~PendingRequest() {
}

// Public methods

CopresenceClient::CopresenceClient(CopresenceClientDelegate* delegate)
    : delegate_(delegate), init_failed_(false), pending_init_operations_(0) {
  DVLOG(3) << "Initializing client.";
  pending_init_operations_++;
  rpc_handler_.reset(new RpcHandler(delegate));
  // We own the RpcHandler, so it won't outlive us.
  rpc_handler_->Initialize(base::Bind(&CopresenceClient::InitStepComplete,
                                      base::Unretained(this),
                                      "Copresence device registration"));

  pending_init_operations_++;
  delegate_->GetWhispernetClient()->Initialize(
      base::Bind(&CopresenceClient::InitStepComplete,
                 // We cannot cancel WhispernetClient initialization.
                 // TODO(ckehoe): Get rid of this.
                 AsWeakPtr(),
                 "Whispernet proxy initialization"));
}

CopresenceClient::~CopresenceClient() {}

// Returns false if any operations were malformed.
void CopresenceClient::ExecuteReportRequest(copresence::ReportRequest request,
                                            const std::string& app_id,
                                            const StatusCallback& callback) {
  // Don't take on any more requests, we can't execute any, init failed.
  if (init_failed_) {
    callback.Run(FAIL);
    return;
  }

  if (pending_init_operations_) {
    pending_requests_queue_.push_back(
        PendingRequest(request, app_id, callback));
  } else {
    rpc_handler_->SendReportRequest(
        make_scoped_ptr(new copresence::ReportRequest(request)),
        app_id,
        callback);
  }
}

// Private methods

void CopresenceClient::CompleteInitialization() {
  if (pending_init_operations_)
    return;

  if (!init_failed_)
    rpc_handler_->ConnectToWhispernet();

  for (std::vector<PendingRequest>::iterator request =
           pending_requests_queue_.begin();
       request != pending_requests_queue_.end();
       ++request) {
    if (init_failed_) {
      request->callback.Run(FAIL);
    } else {
      rpc_handler_->SendReportRequest(
          make_scoped_ptr(new copresence::ReportRequest(request->report)),
          request->app_id,
          request->callback);
    }
  }
  pending_requests_queue_.clear();
}

void CopresenceClient::InitStepComplete(const std::string& step, bool success) {
  if (!success) {
    LOG(ERROR) << step << " failed!";
    init_failed_ = true;
  }

  DVLOG(3) << "Init step: " << step << " complete.";
  pending_init_operations_--;
  CompleteInitialization();
}

}  // namespace copresence
