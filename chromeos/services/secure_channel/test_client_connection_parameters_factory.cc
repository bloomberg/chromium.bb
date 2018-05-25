// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/test_client_connection_parameters_factory.h"

namespace chromeos {

namespace secure_channel {

// static
TestClientConnectionParametersFactory*
TestClientConnectionParametersFactory::Get() {
  static base::NoDestructor<TestClientConnectionParametersFactory> factory;
  return factory.get();
}

TestClientConnectionParametersFactory::TestClientConnectionParametersFactory() =
    default;

TestClientConnectionParametersFactory::
    ~TestClientConnectionParametersFactory() = default;

ClientConnectionParameters TestClientConnectionParametersFactory::Create(
    const std::string& feature) {
  auto fake_connection_delegate = std::make_unique<FakeConnectionDelegate>();
  ClientConnectionParameters parameters(
      feature, fake_connection_delegate->GenerateInterfacePtr());

  client_id_to_delegate_map_[parameters.id()] =
      std::move(fake_connection_delegate);

  return parameters;
}

FakeConnectionDelegate*
TestClientConnectionParametersFactory::GetDelegateForParameters(
    const ClientConnectionParameters& client_connection_parameters) {
  if (!base::ContainsKey(client_id_to_delegate_map_,
                         client_connection_parameters.id())) {
    return nullptr;
  }

  return client_id_to_delegate_map_[client_connection_parameters.id()].get();
}

}  // namespace secure_channel

}  // namespace chromeos
