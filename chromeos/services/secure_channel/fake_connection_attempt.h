// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_CONNECTION_ATTEMPT_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_CONNECTION_ATTEMPT_H_

#include <string>
#include <unordered_map>

#include "base/macros.h"
#include "chromeos/services/secure_channel/connection_attempt.h"
#include "chromeos/services/secure_channel/pending_connection_request.h"

namespace chromeos {

namespace secure_channel {

class ConnectionAttemptDelegate;

// Fake ConnectionAttempt implementation, whose FailureDetailType is
// std::string.
class FakeConnectionAttempt : public ConnectionAttempt<std::string> {
 public:
  FakeConnectionAttempt(ConnectionAttemptDelegate* delegate);
  ~FakeConnectionAttempt() override;

  using IdToRequestMap = std::unordered_map<
      std::string,
      std::unique_ptr<PendingConnectionRequest<std::string>>>;
  const IdToRequestMap& id_to_request_map() const { return id_to_request_map_; }

  // Make OnConnectionAttempt{Succeeded|FinishedWithoutConnection}() public for
  // testing.
  using ConnectionAttempt<std::string>::OnConnectionAttemptSucceeded;
  using ConnectionAttempt<
      std::string>::OnConnectionAttemptFinishedWithoutConnection;

 private:
  void ProcessAddingNewConnectionRequest(
      std::unique_ptr<PendingConnectionRequest<std::string>> request) override;

  IdToRequestMap id_to_request_map_;

  DISALLOW_COPY_AND_ASSIGN(FakeConnectionAttempt);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_CONNECTION_ATTEMPT_H_
