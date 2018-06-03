// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/fake_active_connection_manager.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "chromeos/services/secure_channel/public/cpp/shared/authenticated_channel.h"

namespace chromeos {

namespace secure_channel {

FakeActiveConnectionManager::FakeActiveConnectionManager(
    ActiveConnectionManager::Delegate* delegate)
    : ActiveConnectionManager(delegate) {}

FakeActiveConnectionManager::~FakeActiveConnectionManager() = default;

void FakeActiveConnectionManager::SetDisconnecting(
    const ConnectionDetails& connection_details) {
  DCHECK(base::ContainsKey(connection_details_to_active_metadata_map_,
                           connection_details));

  ConnectionState& state = std::get<0>(
      connection_details_to_active_metadata_map_[connection_details]);
  DCHECK_EQ(ConnectionState::kActiveConnectionExists, state);

  state = ConnectionState::kDisconnectingConnectionExists;
}

void FakeActiveConnectionManager::SetDisconnected(
    const ConnectionDetails& connection_details) {
  DCHECK(base::ContainsKey(connection_details_to_active_metadata_map_,
                           connection_details));
  DCHECK_NE(
      ConnectionState::kNoConnectionExists,
      std::get<0>(
          connection_details_to_active_metadata_map_[connection_details]));

  size_t num_erased =
      connection_details_to_active_metadata_map_.erase(connection_details);
  DCHECK_EQ(1u, num_erased);

  OnChannelDisconnected(connection_details);
}

ActiveConnectionManager::ConnectionState
FakeActiveConnectionManager::GetConnectionState(
    const ConnectionDetails& connection_details) const {
  if (!base::ContainsKey(connection_details_to_active_metadata_map_,
                         connection_details)) {
    return ConnectionState::kNoConnectionExists;
  }

  return std::get<0>(
      connection_details_to_active_metadata_map_.at(connection_details));
}

void FakeActiveConnectionManager::PerformAddActiveConnection(
    std::unique_ptr<AuthenticatedChannel> authenticated_channel,
    std::vector<std::unique_ptr<ClientConnectionParameters>> initial_clients,
    const ConnectionDetails& connection_details) {
  DCHECK(!base::ContainsKey(connection_details_to_active_metadata_map_,
                            connection_details));
  connection_details_to_active_metadata_map_[connection_details] =
      std::make_tuple(ConnectionState::kActiveConnectionExists,
                      std::move(authenticated_channel),
                      std::move(initial_clients));
}

void FakeActiveConnectionManager::PerformAddClientToChannel(
    std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
    const ConnectionDetails& connection_details) {
  DCHECK(base::ContainsKey(connection_details_to_active_metadata_map_,
                           connection_details));
  std::get<2>(connection_details_to_active_metadata_map_[connection_details])
      .push_back(std::move(client_connection_parameters));
}

FakeActiveConnectionManagerDelegate::FakeActiveConnectionManagerDelegate() =
    default;

FakeActiveConnectionManagerDelegate::~FakeActiveConnectionManagerDelegate() =
    default;

void FakeActiveConnectionManagerDelegate::OnDisconnected(
    const ConnectionDetails& connection_details) {
  if (!base::ContainsKey(connection_details_to_num_disconnections_map_,
                         connection_details)) {
    connection_details_to_num_disconnections_map_[connection_details] = 0u;
  }

  ++connection_details_to_num_disconnections_map_[connection_details];
}

}  // namespace secure_channel

}  // namespace chromeos
