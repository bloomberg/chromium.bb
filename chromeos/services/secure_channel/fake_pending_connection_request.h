// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_PENDING_CONNECTION_REQUEST_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_PENDING_CONNECTION_REQUEST_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "chromeos/services/secure_channel/pending_connection_request.h"

namespace chromeos {

namespace secure_channel {

class PendingConnectionRequestDelegate;

// Fake PendingConnectionRequest implementation, whose FailureDetailType is
// std::string.
class FakePendingConnectionRequest
    : public PendingConnectionRequest<std::string> {
 public:
  FakePendingConnectionRequest(PendingConnectionRequestDelegate* delegate);
  ~FakePendingConnectionRequest() override;

  const std::vector<std::string>& handled_failure_details() const {
    return handled_failure_details_;
  }

  // Make NotifyRequestFinishedWithoutConnection() public for testing.
  using PendingConnectionRequest<
      std::string>::NotifyRequestFinishedWithoutConnection;

 private:
  // PendingConnectionRequest<std::string>:
  void HandleConnectionFailure(std::string failure_detail) override;

  std::vector<std::string> handled_failure_details_;

  DISALLOW_COPY_AND_ASSIGN(FakePendingConnectionRequest);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_PENDING_CONNECTION_REQUEST_H_
