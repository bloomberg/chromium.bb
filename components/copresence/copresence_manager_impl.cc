// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence/copresence_manager_impl.h"

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "components/copresence/public/copresence_delegate.h"
#include "components/copresence/public/whispernet_client.h"
#include "components/copresence/rpc/rpc_handler.h"

namespace {

// Number of characters of suffix to log for auth tokens
const int kTokenSuffix = 5;

}  // namespace

namespace copresence {

PendingRequest::PendingRequest(const ReportRequest& report,
                               const std::string& app_id,
                               const std::string& auth_token,
                               const StatusCallback& callback)
    : report(new ReportRequest(report)),
      app_id(app_id),
      auth_token(auth_token),
      callback(callback) {}

PendingRequest::~PendingRequest() {}

// static
scoped_ptr<CopresenceManager> CopresenceManager::Create(
    CopresenceDelegate* delegate) {
  return make_scoped_ptr(new CopresenceManagerImpl(delegate));
}


// Public methods

CopresenceManagerImpl::~CopresenceManagerImpl() {
  whispernet_init_callback_.Cancel();
}

// Returns false if any operations were malformed.
void CopresenceManagerImpl::ExecuteReportRequest(
    const ReportRequest& request,
    const std::string& app_id,
    const StatusCallback& callback) {
  // If initialization has failed, reject all requests.
  if (init_failed_) {
    callback.Run(FAIL);
    return;
  }

  // Check if we are initialized enough to execute this request.
  // If we haven't seen this auth token yet, we need to register for it.
  // TODO(ckehoe): Queue per device ID instead of globally.
  DCHECK(rpc_handler_);
  const std::string& auth_token = delegate_->GetAuthToken();
  if (!rpc_handler_->IsRegisteredForToken(auth_token)) {
    std::string token_str = auth_token.empty() ? "(anonymous)" :
        base::StringPrintf("(token ...%s)",
                           auth_token.substr(auth_token.length() - kTokenSuffix,
                                             kTokenSuffix).c_str());
    rpc_handler_->RegisterForToken(
        auth_token,
        // The manager owns the RpcHandler, so this callback cannot outlive us.
        base::Bind(&CopresenceManagerImpl::InitStepComplete,
                   base::Unretained(this),
                   "Device registration " + token_str));
    pending_init_operations_++;
  }

  // Execute the request if possible, or queue it
  // if initialization is still in progress.
  if (pending_init_operations_) {
    pending_requests_queue_.push_back(
        new PendingRequest(request, app_id, auth_token, callback));
  } else {
    rpc_handler_->SendReportRequest(
        make_scoped_ptr(new ReportRequest(request)),
        app_id,
        auth_token,
        callback);
  }
}

// Private methods

CopresenceManagerImpl::CopresenceManagerImpl(CopresenceDelegate* delegate)
    : init_failed_(false),
      // This callback gets cancelled when we are destroyed.
      whispernet_init_callback_(
          base::Bind(&CopresenceManagerImpl::InitStepComplete,
                     base::Unretained(this),
                     "Whispernet proxy initialization")),
      pending_init_operations_(0),
      delegate_(delegate),
      rpc_handler_(new RpcHandler(delegate)) {
  DCHECK(delegate);
  DCHECK(delegate->GetWhispernetClient());

  delegate->GetWhispernetClient()->Initialize(
      whispernet_init_callback_.callback());
  pending_init_operations_++;
}

void CopresenceManagerImpl::CompleteInitialization() {
  if (pending_init_operations_)
    return;

  DCHECK(rpc_handler_.get());
  if (!init_failed_)
    rpc_handler_->ConnectToWhispernet();

  // Not const because SendReportRequest takes ownership of the ReportRequests.
  // This is ok though, as the entire queue is deleted afterwards.
  for (PendingRequest* request : pending_requests_queue_) {
    if (init_failed_) {
      request->callback.Run(FAIL);
    } else {
      rpc_handler_->SendReportRequest(
          request->report.Pass(),
          request->app_id,
          request->auth_token,
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
    // TODO(ckehoe): Retry for registration failures. But maybe not here.
  }

  DVLOG(3) << step << " complete.";
  DCHECK(pending_init_operations_ > 0);
  pending_init_operations_--;
  CompleteInitialization();
}

}  // namespace copresence
