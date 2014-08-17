// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence/copresence_manager_impl.h"

#include "base/bind.h"
#include "components/copresence/public/copresence_delegate.h"
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

CopresenceManagerImpl::~CopresenceManagerImpl() {}

// Returns false if any operations were malformed.
void CopresenceManagerImpl::ExecuteReportRequest(
    ReportRequest request,
    const std::string& app_id,
    const StatusCallback& callback) {
  // Don't take on any more requests. We can't execute them since init failed.
  if (init_failed_) {
    callback.Run(FAIL);
    return;
  }

  DCHECK(rpc_handler_.get());
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

CopresenceManagerImpl::CopresenceManagerImpl(CopresenceDelegate* delegate)
    : init_failed_(false),
      pending_init_operations_(0),
      delegate_(delegate) {
  DCHECK(delegate);
}

void CopresenceManagerImpl::CompleteInitialization() {
  if (pending_init_operations_)
    return;

  DCHECK(rpc_handler_.get());
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

void CopresenceManagerImpl::InitStepComplete(
    const std::string& step, bool success) {
  if (!success) {
    LOG(ERROR) << step << " failed!";
    init_failed_ = true;
  }

  DVLOG(3) << "Init step: " << step << " complete.";
  pending_init_operations_--;
  CompleteInitialization();
}

}  // namespace copresence
