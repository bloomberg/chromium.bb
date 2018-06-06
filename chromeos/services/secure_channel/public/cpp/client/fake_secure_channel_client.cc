// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/public/cpp/client/fake_secure_channel_client.h"

#include "chromeos/services/secure_channel/public/cpp/client/connection_attempt.h"
#include "components/cryptauth/remote_device_ref.h"

namespace chromeos {

namespace secure_channel {

FakeSecureChannelClient::ConnectionRequestArguments::ConnectionRequestArguments(
    cryptauth::RemoteDeviceRef device_to_connect,
    cryptauth::RemoteDeviceRef local_device,
    const std::string& feature,
    const ConnectionPriority& connection_priority)
    : device_to_connect(device_to_connect),
      local_device(local_device),
      feature(feature),
      connection_priority(connection_priority) {}

FakeSecureChannelClient::ConnectionRequestArguments::
    ~ConnectionRequestArguments() = default;

FakeSecureChannelClient::FakeSecureChannelClient() = default;

FakeSecureChannelClient::~FakeSecureChannelClient() {
  DCHECK(!next_initiate_connection_connection_attempt_);
  DCHECK(!next_listen_for_connection_connection_attempt_);
}

std::unique_ptr<ConnectionAttempt>
FakeSecureChannelClient::InitiateConnectionToDevice(
    cryptauth::RemoteDeviceRef device_to_connect,
    cryptauth::RemoteDeviceRef local_device,
    const std::string& feature,
    ConnectionPriority connection_priority) {
  last_initiate_connection_request_arguments_list_.push_back(
      std::make_unique<ConnectionRequestArguments>(
          device_to_connect, local_device, feature, connection_priority));
  return std::move(next_initiate_connection_connection_attempt_);
}

std::unique_ptr<ConnectionAttempt>
FakeSecureChannelClient::ListenForConnectionFromDevice(
    cryptauth::RemoteDeviceRef device_to_connect,
    cryptauth::RemoteDeviceRef local_device,
    const std::string& feature,
    ConnectionPriority connection_priority) {
  last_listen_for_connection_request_arguments_list_.push_back(
      std::make_unique<ConnectionRequestArguments>(
          device_to_connect, local_device, feature, connection_priority));
  return std::move(next_listen_for_connection_connection_attempt_);
}

}  // namespace secure_channel

}  // namespace chromeos
