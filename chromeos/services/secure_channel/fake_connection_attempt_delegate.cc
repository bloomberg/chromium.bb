// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/fake_connection_attempt_delegate.h"

#include "base/logging.h"
#include "components/cryptauth/secure_channel.h"

namespace chromeos {

namespace secure_channel {

FakeConnectionAttemptDelegate::FakeConnectionAttemptDelegate() = default;

FakeConnectionAttemptDelegate::~FakeConnectionAttemptDelegate() = default;

void FakeConnectionAttemptDelegate::OnConnectionAttemptSucceeded(
    const std::string& attempt_id,
    std::unique_ptr<cryptauth::SecureChannel> secure_channel) {
  DCHECK(attempt_id_.empty());
  DCHECK(!attempt_id.empty());

  attempt_id_ = attempt_id;
  secure_channel_ = std::move(secure_channel);
}

void FakeConnectionAttemptDelegate::
    OnConnectionAttemptFinishedWithoutConnection(
        const std::string& attempt_id) {
  DCHECK(attempt_id_.empty());
  DCHECK(!attempt_id.empty());
  attempt_id_ = attempt_id;
}

}  // namespace secure_channel

}  // namespace chromeos
