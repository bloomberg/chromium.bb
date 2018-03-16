// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FAKE_FIDO_DISCOVERY_H_
#define DEVICE_FIDO_FAKE_FIDO_DISCOVERY_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "device/fido/fido_discovery.h"
#include "device/fido/u2f_transport_protocol.h"

namespace service_manager {
class Connector;
}

namespace device {
namespace test {

// Fake FIDO discovery simulating the behavior of the production
// implementations, and can be used to feed U2fRequests with fake/mock FIDO
// devices.
//
// Most often this class is used together with ScopedFakeFidoDiscoveryFactory:
//
//   ScopedFakeFidoDiscoveryFactory factory;
//   auto* fake_hid_discovery = factory.ForgeNextHidDiscovery();
//   auto* fake_ble_discovery = factory.ForgeNextBleDiscovery();
//
//   // Run the production code that will eventually call:
//   //// FidoDiscovery::Create(U2fTransportProtocol::kUsbHumanInterfaceDevice)
//   //// hid_instance->Start();
//   //// FidoDiscovery::Create(U2fTransportProtocol::kBluetoothLowEnergy)
//   //// ble_instance->Start();
//
//   // Wait, i.e. spin the message loop until the fake discoveries are started.
//   fake_hid_discovery->WaitForCallToStart();
//   fake_ble_discovery->WaitForCallToStart();
//
//   // Add devices to be discovered immediately.
//   fake_hid_discovery->AddDevice(std::make_unique<MockFidoDevice>(...));
//
//   // Start discoveries (HID succeeds, BLE fails).
//   fake_hid_discovery->SimulateStart(true /* success */);
//   fake_ble_discovery->SimulateStart(false /* success */);
//
//   // Add devices discovered after doing some heavy lifting.
//   fake_hid_discovery->AddDevice(std::make_unique<MockFidoDevice>(...));
//
//   // Run the production code that will eventually stop the discovery.
//   //// hid_instance->Stop();
//
//   // Wait for discovery to be stopped by the production code, and simulate
//   // the discovery starting successfully.
//   fake_hid_discovery->WaitForCallToStopAndSimulateSuccess();
//
class FakeFidoDiscovery : public FidoDiscovery,
                          public base::SupportsWeakPtr<FakeFidoDiscovery> {
 public:
  enum class StartStopMode {
    // SimulateStarted()/SimualteStopped() needs to be called manually after the
    // production code calls Start()/Stop().
    kManual,
    // The discovery is automatically and successfully started/stopped once
    // Start()/Stop() is called.
    kAutomatic
  };

  explicit FakeFidoDiscovery(U2fTransportProtocol transport,
                             StartStopMode mode = StartStopMode::kManual);
  ~FakeFidoDiscovery() override;

  // Blocks until start/stop is requested.
  void WaitForCallToStart();
  void WaitForCallToStop();

  // Simulates the discovery actually starting/stopping.
  void SimulateStarted(bool success);
  void SimulateStopped(bool success);

  // Combines WaitForCallToStart/Stop + SimulateStarted/Stopped(true).
  void WaitForCallToStartAndSimulateSuccess();
  void WaitForCallToStopAndSimulateSuccess();

  bool is_start_requested() const { return !start_called_callback_; }
  bool is_stop_requested() const { return !stop_called_callback_; }
  bool is_running() const { return is_running_; }

  // Tests are to directly call Add/RemoveDevice to simulate adding/removing
  // devices. Observers are automatically notified.
  using FidoDiscovery::AddDevice;
  using FidoDiscovery::RemoveDevice;

  // FidoDiscovery:
  U2fTransportProtocol GetTransportProtocol() const override;
  void Start() override;
  void Stop() override;

 private:
  const U2fTransportProtocol transport_;

  const StartStopMode mode_;
  bool is_running_ = false;

  base::RunLoop wait_for_start_loop_;
  base::RunLoop wait_for_stop_loop_;

  base::OnceClosure start_called_callback_;
  base::OnceClosure stop_called_callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeFidoDiscovery);
};

// Overrides FidoDiscovery::Create to construct FakeFidoDiscoveries while this
// instance is in scope.
class ScopedFakeFidoDiscoveryFactory
    : public ::device::internal::ScopedFidoDiscoveryFactory {
 public:
  using StartStopMode = FakeFidoDiscovery::StartStopMode;

  ScopedFakeFidoDiscoveryFactory();
  ~ScopedFakeFidoDiscoveryFactory();

  // Constructs a fake BLE/HID discovery to be returned from the next call to
  // FidoDiscovery::Create. Returns a raw pointer to the fake so that tests can
  // set it up according to taste.
  //
  // It is an error not to call the relevant method prior to a call to
  // FidoDiscovery::Create with the respective transport.
  FakeFidoDiscovery* ForgeNextHidDiscovery(
      StartStopMode mode = StartStopMode::kManual);
  FakeFidoDiscovery* ForgeNextBleDiscovery(
      StartStopMode mode = StartStopMode::kManual);

 protected:
  std::unique_ptr<FidoDiscovery> CreateFidoDiscovery(
      U2fTransportProtocol transport,
      ::service_manager::Connector* connector) override;

 private:
  std::unique_ptr<FakeFidoDiscovery> next_hid_discovery_;
  std::unique_ptr<FakeFidoDiscovery> next_ble_discovery_;

  DISALLOW_COPY_AND_ASSIGN(ScopedFakeFidoDiscoveryFactory);
};

}  // namespace test
}  // namespace device

#endif  // DEVICE_FIDO_FAKE_FIDO_DISCOVERY_H_
