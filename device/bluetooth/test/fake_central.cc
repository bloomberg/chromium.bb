// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_central.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_filter.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "device/bluetooth/public/interfaces/test/fake_bluetooth.mojom.h"
#include "device/bluetooth/test/fake_peripheral.h"

namespace bluetooth {

FakeCentral::FakeCentral(mojom::CentralState state,
                         mojom::FakeCentralRequest request)
    : state_(state), binding_(this, std::move(request)) {}

void FakeCentral::SimulatePreconnectedPeripheral(
    const std::string& address,
    const std::string& name,
    const std::vector<device::BluetoothUUID>& known_service_uuids,
    SimulatePreconnectedPeripheralCallback callback) {
  auto device_iter = devices_.find(address);
  if (device_iter == devices_.end()) {
    auto fake_peripheral = base::MakeUnique<FakePeripheral>(this, address);

    auto insert_iter = devices_.emplace(address, std::move(fake_peripheral));
    DCHECK(insert_iter.second);
    device_iter = insert_iter.first;
  }

  FakePeripheral* fake_peripheral =
      static_cast<FakePeripheral*>(device_iter->second.get());
  fake_peripheral->SetName(name);
  fake_peripheral->SetSystemConnected(true);
  fake_peripheral->SetServiceUUIDs(device::BluetoothDevice::UUIDSet(
      known_service_uuids.begin(), known_service_uuids.end()));

  std::move(callback).Run();
}

void FakeCentral::SetNextGATTConnectionResponse(
    const std::string& address,
    uint16_t code,
    SetNextGATTConnectionResponseCallback callback) {
  auto device_iter = devices_.find(address);
  if (device_iter == devices_.end()) {
    std::move(callback).Run(false);
    return;
  }

  FakePeripheral* fake_peripheral =
      static_cast<FakePeripheral*>(device_iter->second.get());
  fake_peripheral->SetNextGATTConnectionResponse(code);
  std::move(callback).Run(true);
}

void FakeCentral::SetNextGATTDiscoveryResponse(
    const std::string& address,
    uint16_t code,
    SetNextGATTDiscoveryResponseCallback callback) {
  auto device_iter = devices_.find(address);
  if (device_iter == devices_.end()) {
    std::move(callback).Run(false);
  }

  FakePeripheral* fake_peripheral =
      static_cast<FakePeripheral*>(device_iter->second.get());
  fake_peripheral->SetNextGATTDiscoveryResponse(code);
  std::move(callback).Run(true);
}

void FakeCentral::AddFakeService(const std::string& peripheral_address,
                                 const device::BluetoothUUID& service_uuid,
                                 AddFakeServiceCallback callback) {
  auto device_iter = devices_.find(peripheral_address);
  if (device_iter == devices_.end()) {
    std::move(callback).Run(base::nullopt);
  }

  FakePeripheral* fake_peripheral =
      static_cast<FakePeripheral*>(device_iter->second.get());
  std::move(callback).Run(fake_peripheral->AddFakeService(service_uuid));
}

std::string FakeCentral::GetAddress() const {
  NOTREACHED();
  return std::string();
}

std::string FakeCentral::GetName() const {
  NOTREACHED();
  return std::string();
}

void FakeCentral::SetName(const std::string& name,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) {
  NOTREACHED();
}

bool FakeCentral::IsInitialized() const {
  return true;
}

bool FakeCentral::IsPresent() const {
  switch (state_) {
    case mojom::CentralState::ABSENT:
      return false;
    case mojom::CentralState::POWERED_OFF:
    case mojom::CentralState::POWERED_ON:
      return true;
  }
  NOTREACHED();
  return false;
}

bool FakeCentral::IsPowered() const {
  switch (state_) {
    case mojom::CentralState::POWERED_OFF:
      return false;
    case mojom::CentralState::POWERED_ON:
      return true;
    case mojom::CentralState::ABSENT:
      // Clients shouldn't call IsPowered() when the adapter is not present.
      NOTREACHED();
      return false;
  }
  NOTREACHED();
  return false;
}

void FakeCentral::SetPowered(bool powered,
                             const base::Closure& callback,
                             const ErrorCallback& error_callback) {
  NOTREACHED();
}

bool FakeCentral::IsDiscoverable() const {
  NOTREACHED();
  return false;
}

void FakeCentral::SetDiscoverable(bool discoverable,
                                  const base::Closure& callback,
                                  const ErrorCallback& error_callback) {
  NOTREACHED();
}

bool FakeCentral::IsDiscovering() const {
  NOTREACHED();
  return false;
}

FakeCentral::UUIDList FakeCentral::GetUUIDs() const {
  NOTREACHED();
  return UUIDList();
}

void FakeCentral::CreateRfcommService(
    const device::BluetoothUUID& uuid,
    const ServiceOptions& options,
    const CreateServiceCallback& callback,
    const CreateServiceErrorCallback& error_callback) {
  NOTREACHED();
}

void FakeCentral::CreateL2capService(
    const device::BluetoothUUID& uuid,
    const ServiceOptions& options,
    const CreateServiceCallback& callback,
    const CreateServiceErrorCallback& error_callback) {
  NOTREACHED();
}

void FakeCentral::RegisterAdvertisement(
    std::unique_ptr<device::BluetoothAdvertisement::Data> advertisement_data,
    const CreateAdvertisementCallback& callback,
    const AdvertisementErrorCallback& error_callback) {
  NOTREACHED();
}

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
void FakeCentral::SetAdvertisingInterval(
    const base::TimeDelta& min,
    const base::TimeDelta& max,
    const base::Closure& callback,
    const AdvertisementErrorCallback& error_callback) {
  NOTREACHED();
}
#endif

device::BluetoothLocalGattService* FakeCentral::GetGattService(
    const std::string& identifier) const {
  NOTREACHED();
  return nullptr;
}

void FakeCentral::AddDiscoverySession(
    device::BluetoothDiscoveryFilter* discovery_filter,
    const base::Closure& callback,
    const DiscoverySessionErrorCallback& error_callback) {
  NOTREACHED();
}

void FakeCentral::RemoveDiscoverySession(
    device::BluetoothDiscoveryFilter* discovery_filter,
    const base::Closure& callback,
    const DiscoverySessionErrorCallback& error_callback) {
  NOTREACHED();
}

void FakeCentral::SetDiscoveryFilter(
    std::unique_ptr<device::BluetoothDiscoveryFilter> discovery_filter,
    const base::Closure& callback,
    const DiscoverySessionErrorCallback& error_callback) {
  NOTREACHED();
}

void FakeCentral::RemovePairingDelegateInternal(
    device::BluetoothDevice::PairingDelegate* pairing_delegate) {
  NOTREACHED();
}

FakeCentral::~FakeCentral() {}

}  // namespace bluetooth
