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

FakeFidoDiscovery::FakeFidoDiscovery(U2fTransportProtocol transport,
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
      U2fTransportProtocol::kUsbHumanInterfaceDevice, mode);
  return next_hid_discovery_.get();
}

FakeFidoDiscovery* ScopedFakeFidoDiscoveryFactory::ForgeNextBleDiscovery(
    FakeFidoDiscovery::StartMode mode) {
  next_ble_discovery_ = std::make_unique<FakeFidoDiscovery>(
      U2fTransportProtocol::kBluetoothLowEnergy, mode);
  return next_ble_discovery_.get();
}

std::unique_ptr<FidoDiscovery>
ScopedFakeFidoDiscoveryFactory::CreateFidoDiscovery(
    U2fTransportProtocol transport,
    ::service_manager::Connector* connector) {
  switch (transport) {
    case U2fTransportProtocol::kBluetoothLowEnergy:
      DCHECK(next_ble_discovery_);
      return std::move(next_ble_discovery_);
    case U2fTransportProtocol::kUsbHumanInterfaceDevice:
      DCHECK(next_hid_discovery_);
      return std::move(next_hid_discovery_);
  }
  NOTREACHED();
  return nullptr;
}

}  // namespace test

}  // namespace device
