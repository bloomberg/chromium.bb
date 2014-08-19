// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/login/auth/online_attempt_host.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "chromeos/login/auth/auth_attempt_state.h"
#include "chromeos/login/auth/online_attempt.h"
#include "chromeos/login/auth/user_context.h"
#include "components/user_manager/user_type.h"

namespace chromeos {

OnlineAttemptHost::OnlineAttemptHost(Delegate* delegate)
    : message_loop_(base::MessageLoopProxy::current()),
      delegate_(delegate),
      weak_ptr_factory_(this) {
}

OnlineAttemptHost::~OnlineAttemptHost() {
  Reset();
}

void OnlineAttemptHost::Check(net::URLRequestContextGetter* request_context,
                              const UserContext& user_context) {
  if (user_context != current_attempt_user_context_) {
    Reset();
    current_attempt_user_context_ = user_context;

    state_.reset(new AuthAttemptState(user_context,
                                      user_manager::USER_TYPE_REGULAR,
                                      false,    // unlock
                                      false,    // online_complete
                                      false));  // user_is_new
    online_attempt_.reset(new OnlineAttempt(state_.get(), this));
    online_attempt_->Initiate(request_context);
  }
}

void OnlineAttemptHost::Reset() {
  online_attempt_.reset(NULL);
  current_attempt_user_context_ = UserContext();
}

void OnlineAttemptHost::Resolve() {
  if (state_->online_complete()) {
    bool success = state_->online_outcome().reason() == AuthFailure::NONE;
    message_loop_->PostTask(FROM_HERE,
                            base::Bind(&OnlineAttemptHost::ResolveOnUIThread,
                                       weak_ptr_factory_.GetWeakPtr(),
                                       success));
  }
}

void OnlineAttemptHost::ResolveOnUIThread(bool success) {
  delegate_->OnChecked(current_attempt_user_context_.GetUserID(), success);
  Reset();
}

}  // namespace chromeos
