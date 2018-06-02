// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/fake_pending_connection_manager.h"

#include "base/logging.h"
#include "chromeos/services/secure_channel/public/cpp/shared/authenticated_channel.h"

namespace chromeos {

namespace secure_channel {

FakePendingConnectionManager::FakePendingConnectionManager(Delegate* delegate)
    : PendingConnectionManager(delegate) {}

FakePendingConnectionManager::~FakePendingConnectionManager() = default;

void FakePendingConnectionManager::NotifyConnectionForHandledRequests(
    std::unique_ptr<AuthenticatedChannel> authenticated_channel,
    const ConnectionDetails& connection_details) {
  std::vector<std::unique_ptr<ClientConnectionParameters>> client_list;

  auto it = handled_requests_.begin();
  while (it != handled_requests_.end()) {
    if (std::get<0>(*it) != connection_details) {
      ++it;
      continue;
    }

    client_list.push_back(std::move(std::get<1>(*it)));
    it = handled_requests_.erase(it);
  }

  // There must be at least one client in the list.
  DCHECK_LT(0u, client_list.size());

  NotifyOnConnection(std::move(authenticated_channel), std::move(client_list),
                     connection_details);
}

void FakePendingConnectionManager::HandleConnectionRequest(
    const ConnectionDetails& connection_details,
    std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
    ConnectionRole connection_role) {
  handled_requests_.push_back(std::make_tuple(
      connection_details, std::move(client_connection_parameters),
      connection_role));
}

FakePendingConnectionManagerDelegate::FakePendingConnectionManagerDelegate() =
    default;

FakePendingConnectionManagerDelegate::~FakePendingConnectionManagerDelegate() =
    default;

void FakePendingConnectionManagerDelegate::OnConnection(
    std::unique_ptr<AuthenticatedChannel> authenticated_channel,
    std::vector<std::unique_ptr<ClientConnectionParameters>> clients,
    const ConnectionDetails& connection_details) {
  received_connections_list_.push_back(
      std::make_tuple(std::move(authenticated_channel), std::move(clients),
                      connection_details));
}

}  // namespace secure_channel

}  // namespace chromeos
