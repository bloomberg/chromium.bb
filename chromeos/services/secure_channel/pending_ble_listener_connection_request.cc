// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/pending_ble_listener_connection_request.h"

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chromeos/components/proximity_auth/logging/logging.h"

namespace chromeos {

namespace secure_channel {

namespace {
const char kBleListenerReadableRequestTypeForLogging[] = "BLE Listener";
}  // namespace

// static
PendingBleListenerConnectionRequest::Factory*
    PendingBleListenerConnectionRequest::Factory::test_factory_ = nullptr;

// static
PendingBleListenerConnectionRequest::Factory*
PendingBleListenerConnectionRequest::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<PendingBleListenerConnectionRequest::Factory>
      factory;
  return factory.get();
}

// static
void PendingBleListenerConnectionRequest::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

PendingBleListenerConnectionRequest::Factory::~Factory() = default;

std::unique_ptr<PendingConnectionRequest<BleListenerFailureType>>
PendingBleListenerConnectionRequest::Factory::BuildInstance(
    ClientConnectionParameters client_connection_parameters,
    PendingConnectionRequestDelegate* delegate) {
  return base::WrapUnique(new PendingBleListenerConnectionRequest(
      std::move(client_connection_parameters), delegate));
}

PendingBleListenerConnectionRequest::PendingBleListenerConnectionRequest(
    ClientConnectionParameters client_connection_parameters,
    PendingConnectionRequestDelegate* delegate)
    : PendingConnectionRequestBase<BleListenerFailureType>(
          std::move(client_connection_parameters),
          kBleListenerReadableRequestTypeForLogging,
          delegate) {}

PendingBleListenerConnectionRequest::~PendingBleListenerConnectionRequest() =
    default;

void PendingBleListenerConnectionRequest::HandleConnectionFailure(
    BleListenerFailureType failure_detail) {
  switch (failure_detail) {
    case BleListenerFailureType::kAuthenticationError:
      // Authentication errors cannot be solved via a retry. This situation
      // likely means that the keys for this device or the remote device are out
      // of sync.
      StopRequestDueToConnectionFailures(
          mojom::ConnectionAttemptFailureReason::AUTHENTICATION_ERROR);
      break;
    case BleListenerFailureType::kInvalidBeaconSeeds:
      // Valid BeaconSeeds are required for generating BLE scan filters.
      StopRequestDueToConnectionFailures(
          mojom::ConnectionAttemptFailureReason::
              REMOTE_DEVICE_INVALID_BEACON_SEEDS);
      break;
  }
}

}  // namespace secure_channel

}  // namespace chromeos
