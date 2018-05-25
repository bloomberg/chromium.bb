// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_TEST_CLIENT_CONNECTION_PARAMETERS_FACTORY_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_TEST_CLIENT_CONNECTION_PARAMETERS_FACTORY_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/unguessable_token.h"
#include "chromeos/services/secure_channel/client_connection_parameters.h"
#include "chromeos/services/secure_channel/fake_connection_delegate.h"

namespace chromeos {

namespace secure_channel {

// Test utility which provides an easy way to generate
// ClientConnectionParameters.
class TestClientConnectionParametersFactory {
 public:
  static TestClientConnectionParametersFactory* Get();

  ~TestClientConnectionParametersFactory();

  ClientConnectionParameters Create(const std::string& feature);

  // Returns the FakeConnectionDelegate* associated with the
  // mojom::ConnectionDelegatePtr which resides in
  // |client_connection_parameters|. If this function is passed a
  // ClientConnectionParameters that was not created by this factory, nullptr is
  // returned.
  FakeConnectionDelegate* GetDelegateForParameters(
      const ClientConnectionParameters& client_connection_parameters);

 private:
  friend class base::NoDestructor<TestClientConnectionParametersFactory>;

  TestClientConnectionParametersFactory();

  std::unordered_map<base::UnguessableToken,
                     std::unique_ptr<FakeConnectionDelegate>,
                     base::UnguessableTokenHash>
      client_id_to_delegate_map_;

  DISALLOW_COPY_AND_ASSIGN(TestClientConnectionParametersFactory);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_TEST_CLIENT_CONNECTION_PARAMETERS_FACTORY_H_
