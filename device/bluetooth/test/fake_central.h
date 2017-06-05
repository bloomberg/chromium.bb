// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_BLUETOOTH_TEST_FAKE_CENTRAL_H_
#define DEVICE_BLUETOOTH_TEST_FAKE_CENTRAL_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/public/interfaces/test/fake_bluetooth.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace bluetooth {

// Implementation of FakeCentral in
// src/device/bluetooth/public/interfaces/test/fake_bluetooth.mojom.
// Implemented on top of the C++ device/bluetooth API, mainly
// device/bluetooth/bluetooth_adapter.h.
class FakeCentral : NON_EXPORTED_BASE(public mojom::FakeCentral),
                    public device::BluetoothAdapter {
 public:
  FakeCentral(mojom::CentralState state, mojom::FakeCentralRequest request);

  // FakeCentral overrides:
  void SimulatePreconnectedPeripheral(
      const std::string& address,
      const std::string& name,
      const std::vector<device::BluetoothUUID>& known_service_uuids,
      SimulatePreconnectedPeripheralCallback callback) override;

  // BluetoothAdapter overrides:
  std::string GetAddress() const override;
  std::string GetName() const override;
  void SetName(const std::string& name,
               const base::Closure& callback,
               const ErrorCallback& error_callback) override;
  bool IsInitialized() const override;
  bool IsPresent() const override;
  bool IsPowered() const override;
  void SetPowered(bool powered,
                  const base::Closure& callback,
                  const ErrorCallback& error_callback) override;
  bool IsDiscoverable() const override;
  void SetDiscoverable(bool discoverable,
                       const base::Closure& callback,
                       const ErrorCallback& error_callback) override;
  bool IsDiscovering() const override;
  UUIDList GetUUIDs() const override;
  void CreateRfcommService(
      const device::BluetoothUUID& uuid,
      const ServiceOptions& options,
      const CreateServiceCallback& callback,
      const CreateServiceErrorCallback& error_callback) override;
  void CreateL2capService(
      const device::BluetoothUUID& uuid,
      const ServiceOptions& options,
      const CreateServiceCallback& callback,
      const CreateServiceErrorCallback& error_callback) override;
  void RegisterAdvertisement(
      std::unique_ptr<device::BluetoothAdvertisement::Data> advertisement_data,
      const CreateAdvertisementCallback& callback,
      const AdvertisementErrorCallback& error_callback) override;
#if defined(OS_CHROMEOS) || defined(OS_LINUX)
  void SetAdvertisingInterval(
      const base::TimeDelta& min,
      const base::TimeDelta& max,
      const base::Closure& callback,
      const AdvertisementErrorCallback& error_callback) override;
#endif
  device::BluetoothLocalGattService* GetGattService(
      const std::string& identifier) const override;
  void AddDiscoverySession(
      device::BluetoothDiscoveryFilter* discovery_filter,
      const base::Closure& callback,
      const DiscoverySessionErrorCallback& error_callback) override;
  void RemoveDiscoverySession(
      device::BluetoothDiscoveryFilter* discovery_filter,
      const base::Closure& callback,
      const DiscoverySessionErrorCallback& error_callback) override;
  void SetDiscoveryFilter(
      std::unique_ptr<device::BluetoothDiscoveryFilter> discovery_filter,
      const base::Closure& callback,
      const DiscoverySessionErrorCallback& error_callback) override;
  void RemovePairingDelegateInternal(
      device::BluetoothDevice::PairingDelegate* pairing_delegate) override;

 private:
  ~FakeCentral() override;

  mojom::CentralState state_;
  mojo::Binding<mojom::FakeCentral> binding_;
};

}  // namespace bluetooth

#endif  // DEVICE_BLUETOOTH_TEST_FAKE_CENTRAL_H_
