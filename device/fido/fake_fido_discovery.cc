// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fake_fido_discovery.h"

#include <utility>

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {
namespace test {

// FakeFidoDiscovery ----------------------------------------------------------

FakeFidoDiscovery::FakeFidoDiscovery(U2fTransportProtocol transport,
                                     StartStopMode mode)
    : transport_(transport),
      mode_(mode),
      start_called_callback_(wait_for_start_loop_.QuitClosure()),
      stop_called_callback_(wait_for_stop_loop_.QuitClosure()) {}
FakeFidoDiscovery::~FakeFidoDiscovery() = default;

void FakeFidoDiscovery::WaitForCallToStart() {
  wait_for_start_loop_.Run();
  ASSERT_FALSE(start_called_callback_);
}

void FakeFidoDiscovery::WaitForCallToStop() {
  wait_for_stop_loop_.Run();
  ASSERT_FALSE(stop_called_callback_);
}

void FakeFidoDiscovery::SimulateStarted(bool success) {
  ASSERT_FALSE(is_running_);
  is_running_ = success;
  NotifyDiscoveryStarted(success);
}

void FakeFidoDiscovery::SimulateStopped(bool success) {
  ASSERT_TRUE(is_running_);
  is_running_ = !success;
  NotifyDiscoveryStopped(success);
}

void FakeFidoDiscovery::WaitForCallToStartAndSimulateSuccess() {
  WaitForCallToStart();
  SimulateStarted(true /* success */);
}

void FakeFidoDiscovery::WaitForCallToStopAndSimulateSuccess() {
  WaitForCallToStop();
  SimulateStopped(true /* success */);
}

U2fTransportProtocol FakeFidoDiscovery::GetTransportProtocol() const {
  return transport_;
}

void FakeFidoDiscovery::Start() {
  if (is_running_)
    return;

  ASSERT_TRUE(start_called_callback_) << "Start called twice.";
  std::move(start_called_callback_).Run();

  if (mode_ == StartStopMode::kAutomatic)
    SimulateStarted(true /* success */);
}

void FakeFidoDiscovery::Stop() {
  if (!is_running_)
    return;

  ASSERT_TRUE(stop_called_callback_) << "Stop called twice.";
  std::move(stop_called_callback_).Run();

  if (mode_ == StartStopMode::kAutomatic)
    SimulateStopped(true /* success */);
}

// ScopedFakeFidoDiscoveryFactory ---------------------------------------------

ScopedFakeFidoDiscoveryFactory::ScopedFakeFidoDiscoveryFactory() = default;
ScopedFakeFidoDiscoveryFactory::~ScopedFakeFidoDiscoveryFactory() = default;

FakeFidoDiscovery* ScopedFakeFidoDiscoveryFactory::ForgeNextHidDiscovery(
    FakeFidoDiscovery::StartStopMode mode) {
  next_hid_discovery_ = std::make_unique<FakeFidoDiscovery>(
      U2fTransportProtocol::kUsbHumanInterfaceDevice, mode);
  return next_hid_discovery_.get();
}

FakeFidoDiscovery* ScopedFakeFidoDiscoveryFactory::ForgeNextBleDiscovery(
    FakeFidoDiscovery::StartStopMode mode) {
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
