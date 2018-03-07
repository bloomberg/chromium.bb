// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FAKE_U2F_DISCOVERY_H_
#define DEVICE_FIDO_FAKE_U2F_DISCOVERY_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "device/fido/u2f_discovery.h"
#include "device/fido/u2f_transport_protocol.h"

namespace service_manager {
class Connector;
}

namespace device {
namespace test {

// Fake U2F discovery simulating the behavior of the production implementations,
// and can be used to feed U2fRequests with fake/mock U2F devices.
class FakeU2fDiscovery : public U2fDiscovery,
                         public base::SupportsWeakPtr<FakeU2fDiscovery> {
 public:
  explicit FakeU2fDiscovery(U2fTransportProtocol transport);
  ~FakeU2fDiscovery() override;

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
  using U2fDiscovery::AddDevice;
  using U2fDiscovery::RemoveDevice;

  // U2fDiscovery:
  U2fTransportProtocol GetTransportProtocol() const override;
  void Start() override;
  void Stop() override;

 private:
  U2fTransportProtocol transport_;

  bool is_running_ = false;

  base::RunLoop wait_for_start_loop_;
  base::RunLoop wait_for_stop_loop_;

  base::OnceClosure start_called_callback_;
  base::OnceClosure stop_called_callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeU2fDiscovery);
};

// Hijacks the U2fDiscovery::Create() factory method to return user-defined
// U2fDiscovery instances while this instance is in scope.
class ScopedFakeU2fDiscoveryFactory {
 public:
  ScopedFakeU2fDiscoveryFactory();
  ~ScopedFakeU2fDiscoveryFactory();

  // Constructs a fake BLE/HID discovery to be returned from the next call to
  // U2fDiscovery::Create. Returns a raw pointer to the fake so that tests can
  // set it up according to taste.
  FakeU2fDiscovery* ForgeNextBleDiscovery();
  FakeU2fDiscovery* ForgeNextHidDiscovery();

  // Same as above, but the test can supply whatever custom |discovery| instance
  // that it wants to be returned from the next call to U2fDiscovery::Create.
  U2fDiscovery* SetNextHidDiscovery(std::unique_ptr<U2fDiscovery> discovery);
  U2fDiscovery* SetNextBleDiscovery(std::unique_ptr<U2fDiscovery> discovery);

 private:
  static std::unique_ptr<U2fDiscovery> CreateFakeU2fDiscovery(
      U2fTransportProtocol transport,
      ::service_manager::Connector* connector);

  static ScopedFakeU2fDiscoveryFactory* g_current_factory;

  U2fDiscovery::FactoryFuncPtr original_factory_func_;

  std::unique_ptr<U2fDiscovery> next_hid_discovery_;
  std::unique_ptr<U2fDiscovery> next_ble_discovery_;

  DISALLOW_COPY_AND_ASSIGN(ScopedFakeU2fDiscoveryFactory);
};

}  // namespace test
}  // namespace device

#endif  // DEVICE_FIDO_FAKE_U2F_DISCOVERY_H_
