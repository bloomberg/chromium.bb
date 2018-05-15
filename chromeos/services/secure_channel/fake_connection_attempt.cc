// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/fake_connection_attempt.h"

#include "base/stl_util.h"

namespace chromeos {

namespace secure_channel {

FakeConnectionAttempt::FakeConnectionAttempt(
    ConnectionAttemptDelegate* delegate)
    : ConnectionAttempt<std::string>(delegate) {}

FakeConnectionAttempt::~FakeConnectionAttempt() = default;

void FakeConnectionAttempt::ProcessAddingNewConnectionRequest(
    std::unique_ptr<PendingConnectionRequest<std::string>> request) {
  DCHECK(request);
  DCHECK(!base::ContainsKey(id_to_request_map_, request->request_id()));

  id_to_request_map_[request->request_id()] = std::move(request);
}

}  // namespace secure_channel

}  // namespace chromeos
