// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/public/cpp/client/connection_attempt.h"

namespace chromeos {

namespace secure_channel {

ConnectionAttempt::Delegate::~Delegate() = default;

ConnectionAttempt::ConnectionAttempt() = default;

ConnectionAttempt::~ConnectionAttempt() = default;

void ConnectionAttempt::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

void ConnectionAttempt::NotifyConnectionAttemptFailure(
    mojom::ConnectionAttemptFailureReason reason) {
  if (delegate_) {
    delegate_->OnConnectionAttemptFailure(reason);
  } else {
    PA_LOG(ERROR) << "NotifyConnectionAttemptFailure: No delegate added.";
    NOTREACHED();
  }
}

void ConnectionAttempt::NotifyConnection(
    std::unique_ptr<ClientChannel> channel) {
  if (delegate_) {
    delegate_->OnConnection(std::move(channel));
  } else {
    PA_LOG(ERROR) << "NotifyConnection: No delegate added.";
    NOTREACHED();
  }
}

}  // namespace secure_channel

}  // namespace chromeos
