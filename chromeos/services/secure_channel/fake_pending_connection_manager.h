// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_PENDING_CONNECTION_MANAGER_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_PENDING_CONNECTION_MANAGER_H_

#include <tuple>
#include <vector>

#include "base/macros.h"
#include "chromeos/services/secure_channel/client_connection_parameters.h"
#include "chromeos/services/secure_channel/connection_details.h"
#include "chromeos/services/secure_channel/connection_role.h"
#include "chromeos/services/secure_channel/pending_connection_manager.h"

namespace chromeos {

namespace secure_channel {

// Test PendingConnectionManager implementation.
class FakePendingConnectionManager : public PendingConnectionManager {
 public:
  FakePendingConnectionManager(Delegate* delegate);
  ~FakePendingConnectionManager() override;

  using HandledRequestsList = std::vector<std::tuple<ConnectionDetails,
                                                     ClientConnectionParameters,
                                                     ConnectionRole>>;
  HandledRequestsList& handled_requests() { return handled_requests_; }

  // Make NotifyOnConnection() public for testing.
  using PendingConnectionManager::NotifyOnConnection;

 private:
  void HandleConnectionRequest(
      const ConnectionDetails& connection_details,
      ClientConnectionParameters client_connection_parameters,
      ConnectionRole connection_role) override;

  HandledRequestsList handled_requests_;

  DISALLOW_COPY_AND_ASSIGN(FakePendingConnectionManager);
};

// Test PendingConnectionManager::Delegate implementation.
class FakePendingConnectionManagerDelegate
    : public PendingConnectionManager::Delegate {
 public:
  FakePendingConnectionManagerDelegate();
  ~FakePendingConnectionManagerDelegate() override;

  using ReceivedConnectionsList =
      std::vector<std::tuple<std::unique_ptr<AuthenticatedChannel>,
                             std::vector<ClientConnectionParameters>,
                             ConnectionDetails>>;
  ReceivedConnectionsList& received_connections_list() {
    return received_connections_list_;
  }

 private:
  void OnConnection(std::unique_ptr<AuthenticatedChannel> authenticated_channel,
                    std::vector<ClientConnectionParameters> clients,
                    const ConnectionDetails& connection_details) override;

  ReceivedConnectionsList received_connections_list_;

  DISALLOW_COPY_AND_ASSIGN(FakePendingConnectionManagerDelegate);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_PENDING_CONNECTION_MANAGER_H_
