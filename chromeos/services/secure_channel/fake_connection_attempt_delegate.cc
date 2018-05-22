// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/fake_connection_attempt_delegate.h"

#include "base/logging.h"
#include "chromeos/services/secure_channel/authenticated_channel.h"

namespace chromeos {

namespace secure_channel {

FakeConnectionAttemptDelegate::FakeConnectionAttemptDelegate() = default;

FakeConnectionAttemptDelegate::~FakeConnectionAttemptDelegate() = default;

void FakeConnectionAttemptDelegate::OnConnectionAttemptSucceeded(
    const base::UnguessableToken& attempt_id,
    std::unique_ptr<AuthenticatedChannel> authenticated_channel) {
  DCHECK(attempt_id_.is_empty());
  DCHECK(!attempt_id.is_empty());

  attempt_id_ = attempt_id;
  authenticated_channel_ = std::move(authenticated_channel);
}

void FakeConnectionAttemptDelegate::
    OnConnectionAttemptFinishedWithoutConnection(
        const base::UnguessableToken& attempt_id) {
  DCHECK(attempt_id_.is_empty());
  DCHECK(!attempt_id.is_empty());
  attempt_id_ = attempt_id;
}

}  // namespace secure_channel

}  // namespace chromeos
