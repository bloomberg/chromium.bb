// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_PENDING_CONNECTION_REQUEST_DELEGATE_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_PENDING_CONNECTION_REQUEST_DELEGATE_H_

#include <unordered_map>

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "chromeos/services/secure_channel/pending_connection_request_delegate.h"

namespace chromeos {

namespace secure_channel {

// Test PendingConnectionRequestDelegate implementation.
class FakePendingConnectionRequestDelegate
    : public PendingConnectionRequestDelegate {
 public:
  FakePendingConnectionRequestDelegate();
  ~FakePendingConnectionRequestDelegate() override;

  const base::Optional<FailedConnectionReason>& GetFailedConnectionReasonForId(
      const base::UnguessableToken& request_id);

  void set_closure_for_next_delegate_callback(base::OnceClosure closure) {
    closure_for_next_delegate_callback_ = std::move(closure);
  }

 private:
  // PendingConnectionRequestDelegate:
  void OnRequestFinishedWithoutConnection(
      const base::UnguessableToken& request_id,
      FailedConnectionReason reason) override;

  std::unordered_map<base::UnguessableToken,
                     base::Optional<FailedConnectionReason>,
                     base::UnguessableTokenHash>
      request_id_to_failed_connection_reason_map_;

  base::OnceClosure closure_for_next_delegate_callback_;

  DISALLOW_COPY_AND_ASSIGN(FakePendingConnectionRequestDelegate);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_PENDING_CONNECTION_REQUEST_DELEGATE_H_
