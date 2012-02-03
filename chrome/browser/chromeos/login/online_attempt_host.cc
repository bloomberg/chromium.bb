// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/online_attempt_host.h"

#include "base/sha1.h"
#include "chrome/browser/chromeos/login/auth_attempt_state.h"
#include "chrome/browser/chromeos/login/authenticator.h"
#include "chrome/browser/chromeos/login/online_attempt.h"
#include "chrome/browser/profiles/profile.h"

namespace chromeos {

OnlineAttemptHost::OnlineAttemptHost(Delegate* delegate)
    : delegate_(delegate) {
}

OnlineAttemptHost::~OnlineAttemptHost() {
  Reset();
}

void OnlineAttemptHost::Check(Profile* profile,
                              const std::string& username,
                              const std::string& password) {
  std::string attempt_hash = base::SHA1HashString(username + "\n" + password);
  if (attempt_hash != current_attempt_hash_) {
    Reset();
    current_attempt_hash_ = attempt_hash;
    current_username_ = username;

    state_.reset(
        new AuthAttemptState(
            Authenticator::Canonicalize(username),
            password,
            std::string(),
            std::string(),
            std::string(),
            false));  // Isn't a new user.
    online_attempt_ = new OnlineAttempt(false,  // Don't use oauth.
                                        state_.get(),
                                        this);
    online_attempt_->Initiate(profile);
  }
}

void OnlineAttemptHost::Reset() {
  online_attempt_ = NULL;
  current_attempt_hash_.clear();
  current_username_.clear();
}

void OnlineAttemptHost::Resolve() {
  if (state_->online_complete()) {
    bool success = state_->online_outcome().reason() == LoginFailure::NONE;
    delegate_->OnChecked(current_username_, success);
    Reset();
  }
}

}  // chromeos
