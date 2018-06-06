// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_FAKE_SECURE_CHANNEL_CLIENT_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_FAKE_SECURE_CHANNEL_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "chromeos/services/secure_channel/public/cpp/client/secure_channel_client.h"
#include "chromeos/services/secure_channel/public/cpp/shared/authenticated_channel.h"

namespace chromeos {

namespace secure_channel {

// Test SecureChannelClient implementation.
class FakeSecureChannelClient : public SecureChannelClient {
 public:
  struct ConnectionRequestArguments {
   public:
    ConnectionRequestArguments(cryptauth::RemoteDeviceRef device_to_connect,
                               cryptauth::RemoteDeviceRef local_device,
                               const std::string& feature,
                               const ConnectionPriority& connection_priority);
    ~ConnectionRequestArguments();

    cryptauth::RemoteDeviceRef device_to_connect;
    cryptauth::RemoteDeviceRef local_device;
    std::string feature;
    ConnectionPriority connection_priority;

   private:
    DISALLOW_COPY_AND_ASSIGN(ConnectionRequestArguments);
  };

  FakeSecureChannelClient();
  ~FakeSecureChannelClient() override;

  void set_initiate_connection_connection_attempt(
      std::unique_ptr<ConnectionAttempt> attempt) {
    next_initiate_connection_connection_attempt_ = std::move(attempt);
  }

  void set_listen_for_connection_connection_attempt(
      std::unique_ptr<ConnectionAttempt> attempt) {
    next_listen_for_connection_connection_attempt_ = std::move(attempt);
  }

  std::vector<ConnectionRequestArguments*>
  last_initiate_connection_request_arguments_list() {
    std::vector<ConnectionRequestArguments*> arguments_list_raw_;
    std::transform(last_initiate_connection_request_arguments_list_.begin(),
                   last_initiate_connection_request_arguments_list_.end(),
                   std::back_inserter(arguments_list_raw_),
                   [](const auto& arguments) { return arguments.get(); });
    return arguments_list_raw_;
  }

  std::vector<ConnectionRequestArguments*>
  last_listen_for_connection_request_arguments_list() {
    std::vector<ConnectionRequestArguments*> arguments_list_raw_;
    std::transform(last_listen_for_connection_request_arguments_list_.begin(),
                   last_listen_for_connection_request_arguments_list_.end(),
                   std::back_inserter(arguments_list_raw_),
                   [](const auto& arguments) { return arguments.get(); });
    return arguments_list_raw_;
  }

  // SecureChannelClient:
  std::unique_ptr<ConnectionAttempt> InitiateConnectionToDevice(
      cryptauth::RemoteDeviceRef device_to_connect,
      cryptauth::RemoteDeviceRef local_device,
      const std::string& feature,
      ConnectionPriority connection_priority) override;
  std::unique_ptr<ConnectionAttempt> ListenForConnectionFromDevice(
      cryptauth::RemoteDeviceRef device_to_connect,
      cryptauth::RemoteDeviceRef local_device,
      const std::string& feature,
      ConnectionPriority connection_priority) override;

 private:
  std::unique_ptr<ConnectionAttempt>
      next_initiate_connection_connection_attempt_;
  std::unique_ptr<ConnectionAttempt>
      next_listen_for_connection_connection_attempt_;

  std::vector<std::unique_ptr<ConnectionRequestArguments>>
      last_initiate_connection_request_arguments_list_;
  std::vector<std::unique_ptr<ConnectionRequestArguments>>
      last_listen_for_connection_request_arguments_list_;

  DISALLOW_COPY_AND_ASSIGN(FakeSecureChannelClient);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_FAKE_SECURE_CHANNEL_CLIENT_H_
