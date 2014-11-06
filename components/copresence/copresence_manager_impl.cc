// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence/copresence_manager_impl.h"

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/timer/timer.h"
#include "components/copresence/handlers/directive_handler.h"
#include "components/copresence/proto/rpcs.pb.h"
#include "components/copresence/public/whispernet_client.h"
#include "components/copresence/rpc/rpc_handler.h"

namespace {

// Number of characters of suffix to log for auth tokens
const int kTokenSuffix = 5;
const int kPollTimerIntervalMs = 3000;   // milliseconds.
const int kAudioCheckIntervalMs = 1000;  // milliseconds.

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


// Public functions.

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


// Private functions.

CopresenceManagerImpl::CopresenceManagerImpl(CopresenceDelegate* delegate)
    : delegate_(delegate),
      pending_init_operations_(0),
      // This callback gets cancelled when we are destroyed.
      whispernet_init_callback_(
          base::Bind(&CopresenceManagerImpl::InitStepComplete,
                     base::Unretained(this),
                     "Whispernet proxy initialization")),
      init_failed_(false),
      directive_handler_(new DirectiveHandler),
      poll_timer_(new base::RepeatingTimer<CopresenceManagerImpl>),
      audio_check_timer_(new base::RepeatingTimer<CopresenceManagerImpl>) {
  DCHECK(delegate);
  DCHECK(delegate->GetWhispernetClient());

  rpc_handler_.reset(new RpcHandler(delegate, directive_handler_.get()));
  delegate->GetWhispernetClient()->Initialize(
      whispernet_init_callback_.callback());
  pending_init_operations_++;
}

void CopresenceManagerImpl::CompleteInitialization() {
  if (pending_init_operations_)
    return;

  if (!init_failed_) {
    // We destroy |directive_handler_| before |rpc_handler_|, hence passing
    // in |rpc_handler_|'s pointer is safe here.
    directive_handler_->Start(delegate_->GetWhispernetClient(),
                              base::Bind(&RpcHandler::ReportTokens,
                                         base::Unretained(rpc_handler_.get())));

    // Start up timers.
    poll_timer_->Start(FROM_HERE,
                       base::TimeDelta::FromMilliseconds(kPollTimerIntervalMs),
                       base::Bind(&CopresenceManagerImpl::PollForMessages,
                                  base::Unretained(this)));
    audio_check_timer_->Start(
        FROM_HERE, base::TimeDelta::FromMilliseconds(kAudioCheckIntervalMs),
        base::Bind(&CopresenceManagerImpl::AudioCheck, base::Unretained(this)));
  }

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

void CopresenceManagerImpl::PollForMessages() {
  // Report our currently playing tokens.
  const std::string& audible_token =
      directive_handler_->GetCurrentAudioToken(AUDIBLE);
  const std::string& inaudible_token =
      directive_handler_->GetCurrentAudioToken(INAUDIBLE);

  std::vector<AudioToken> tokens;
  if (!audible_token.empty())
    tokens.push_back(AudioToken(audible_token, true));
  if (!inaudible_token.empty())
    tokens.push_back(AudioToken(inaudible_token, false));

  if (!tokens.empty())
    rpc_handler_->ReportTokens(tokens);
}

void CopresenceManagerImpl::AudioCheck() {
  if (!directive_handler_->GetCurrentAudioToken(AUDIBLE).empty() &&
      !directive_handler_->IsAudioTokenHeard(AUDIBLE)) {
    delegate_->HandleStatusUpdate(AUDIO_FAIL);
  } else if (!directive_handler_->GetCurrentAudioToken(INAUDIBLE).empty() &&
             !directive_handler_->IsAudioTokenHeard(INAUDIBLE)) {
    delegate_->HandleStatusUpdate(AUDIO_FAIL);
  }
}

}  // namespace copresence
