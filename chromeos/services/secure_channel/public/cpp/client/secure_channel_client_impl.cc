// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/public/cpp/client/secure_channel_client_impl.h"

#include "base/no_destructor.h"
#include "chromeos/services/secure_channel/public/cpp/client/connection_attempt.h"

namespace chromeos {

namespace secure_channel {

// static
SecureChannelClientImpl::Factory*
    SecureChannelClientImpl::Factory::test_factory_ = nullptr;

// static
SecureChannelClientImpl::Factory* SecureChannelClientImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<Factory> factory;
  return factory.get();
}

// static
void SecureChannelClientImpl::Factory::SetInstanceForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

SecureChannelClientImpl::Factory::~Factory() = default;

std::unique_ptr<SecureChannelClient>
SecureChannelClientImpl::Factory::BuildInstance() {
  return base::WrapUnique(new SecureChannelClientImpl());
}

SecureChannelClientImpl::SecureChannelClientImpl() = default;

SecureChannelClientImpl::~SecureChannelClientImpl() = default;

std::unique_ptr<ConnectionAttempt>
SecureChannelClientImpl::InitiateConnectionToDevice(
    cryptauth::RemoteDeviceRef device_to_connect,
    cryptauth::RemoteDeviceRef local_device,
    const std::string& feature,
    ConnectionPriority connection_priority) {
  return nullptr;
}

std::unique_ptr<ConnectionAttempt>
SecureChannelClientImpl::ListenForConnectionFromDevice(
    cryptauth::RemoteDeviceRef device_to_connect,
    cryptauth::RemoteDeviceRef local_device,
    const std::string& feature,
    ConnectionPriority connection_priority) {
  return nullptr;
}

}  // namespace secure_channel

}  // namespace chromeos
