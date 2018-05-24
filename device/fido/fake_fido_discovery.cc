// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fake_fido_discovery.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {
namespace test {

// FakeFidoDiscovery ----------------------------------------------------------

FakeFidoDiscovery::FakeFidoDiscovery(FidoTransportProtocol transport,
                                     StartMode mode)
    : FidoDiscovery(transport), mode_(mode) {}
FakeFidoDiscovery::~FakeFidoDiscovery() = default;

void FakeFidoDiscovery::WaitForCallToStart() {
  wait_for_start_loop_.Run();
}

void FakeFidoDiscovery::SimulateStarted(bool success) {
  ASSERT_FALSE(is_running());
  NotifyDiscoveryStarted(success);
}

void FakeFidoDiscovery::WaitForCallToStartAndSimulateSuccess() {
  WaitForCallToStart();
  SimulateStarted(true /* success */);
}

void FakeFidoDiscovery::StartInternal() {
  wait_for_start_loop_.Quit();

  if (mode_ == StartMode::kAutomatic) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&FakeFidoDiscovery::SimulateStarted,
                                  base::Unretained(this), true /* success */));
  }
}

// ScopedFakeFidoDiscoveryFactory ---------------------------------------------

ScopedFakeFidoDiscoveryFactory::ScopedFakeFidoDiscoveryFactory() = default;
ScopedFakeFidoDiscoveryFactory::~ScopedFakeFidoDiscoveryFactory() = default;

FakeFidoDiscovery* ScopedFakeFidoDiscoveryFactory::ForgeNextHidDiscovery(
    FakeFidoDiscovery::StartMode mode) {
  next_hid_discovery_ = std::make_unique<FakeFidoDiscovery>(
      FidoTransportProtocol::kUsbHumanInterfaceDevice, mode);
  return next_hid_discovery_.get();
}

FakeFidoDiscovery* ScopedFakeFidoDiscoveryFactory::ForgeNextNfcDiscovery(
    FakeFidoDiscovery::StartMode mode) {
  next_nfc_discovery_ = std::make_unique<FakeFidoDiscovery>(
      FidoTransportProtocol::kNearFieldCommunication, mode);
  return next_nfc_discovery_.get();
}

FakeFidoDiscovery* ScopedFakeFidoDiscoveryFactory::ForgeNextBleDiscovery(
    FakeFidoDiscovery::StartMode mode) {
  next_ble_discovery_ = std::make_unique<FakeFidoDiscovery>(
      FidoTransportProtocol::kBluetoothLowEnergy, mode);
  return next_ble_discovery_.get();
}

FakeFidoDiscovery* ScopedFakeFidoDiscoveryFactory::ForgeNextCableDiscovery(
    FakeFidoDiscovery::StartMode mode) {
  next_cable_discovery_ = std::make_unique<FakeFidoDiscovery>(
      FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy, mode);
  return next_cable_discovery_.get();
}

std::unique_ptr<FidoDiscovery>
ScopedFakeFidoDiscoveryFactory::CreateFidoDiscovery(
    FidoTransportProtocol transport,
    ::service_manager::Connector* connector) {
  switch (transport) {
    case FidoTransportProtocol::kUsbHumanInterfaceDevice:
      return std::move(next_hid_discovery_);
    case FidoTransportProtocol::kNearFieldCommunication:
      return std::move(next_nfc_discovery_);
    case FidoTransportProtocol::kBluetoothLowEnergy:
      return std::move(next_ble_discovery_);
    case FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy:
      return std::move(next_cable_discovery_);
    case FidoTransportProtocol::kInternal:
      NOTREACHED() << "Internal authenticators should be handled separately.";
      return nullptr;
  }
  NOTREACHED();
  return nullptr;
}

}  // namespace test

}  // namespace device
