// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/fake_pending_connection_request.h"

namespace chromeos {

namespace secure_channel {

FakePendingConnectionRequest::FakePendingConnectionRequest(
    PendingConnectionRequestDelegate* delegate)
    : PendingConnectionRequest<std::string>(delegate) {}

FakePendingConnectionRequest::~FakePendingConnectionRequest() = default;

void FakePendingConnectionRequest::HandleConnectionFailure(
    std::string failure_detail) {
  handled_failure_details_.push_back(failure_detail);
}

std::pair<std::string, mojom::ConnectionDelegatePtr>
FakePendingConnectionRequest::ExtractClientData() {
  return std::move(client_data_for_extraction_);
}

}  // namespace secure_channel

}  // namespace chromeos
