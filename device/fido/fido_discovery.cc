// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_discovery.h"

#include <utility>

#include "base/logging.h"
#include "build/build_config.h"
#include "device/fido/fido_device.h"
#include "device/fido/u2f_ble_discovery.h"

// HID is not supported on Android.
#if !defined(OS_ANDROID)
#include "device/fido/fido_hid_discovery.h"
#endif  // !defined(OS_ANDROID)

namespace device {

namespace {

std::unique_ptr<FidoDiscovery> CreateFidoDiscoveryImpl(
    U2fTransportProtocol transport,
    service_manager::Connector* connector) {
  std::unique_ptr<FidoDiscovery> discovery;
  switch (transport) {
    case U2fTransportProtocol::kUsbHumanInterfaceDevice:
#if !defined(OS_ANDROID)
      DCHECK(connector);
      discovery = std::make_unique<FidoHidDiscovery>(connector);
#else
      NOTREACHED() << "USB HID not supported on Android.";
#endif  // !defined(OS_ANDROID)
      break;
    case U2fTransportProtocol::kBluetoothLowEnergy:
      discovery = std::make_unique<U2fBleDiscovery>();
      break;
  }

  DCHECK(discovery);
  DCHECK_EQ(discovery->GetTransportProtocol(), transport);
  return discovery;
}

}  // namespace

FidoDiscovery::Observer::~Observer() = default;

// static
FidoDiscovery::FactoryFuncPtr FidoDiscovery::g_factory_func_ =
    &CreateFidoDiscoveryImpl;

// static
std::unique_ptr<FidoDiscovery> FidoDiscovery::Create(
    U2fTransportProtocol transport,
    service_manager::Connector* connector) {
  return (*g_factory_func_)(transport, connector);
}

FidoDiscovery::FidoDiscovery() = default;

FidoDiscovery::~FidoDiscovery() = default;

void FidoDiscovery::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FidoDiscovery::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FidoDiscovery::NotifyDiscoveryStarted(bool success) {
  for (auto& observer : observers_)
    observer.DiscoveryStarted(this, success);
}

void FidoDiscovery::NotifyDiscoveryStopped(bool success) {
  for (auto& observer : observers_)
    observer.DiscoveryStopped(this, success);
}

void FidoDiscovery::NotifyDeviceAdded(FidoDevice* device) {
  for (auto& observer : observers_)
    observer.DeviceAdded(this, device);
}

void FidoDiscovery::NotifyDeviceRemoved(FidoDevice* device) {
  for (auto& observer : observers_)
    observer.DeviceRemoved(this, device);
}

std::vector<FidoDevice*> FidoDiscovery::GetDevices() {
  std::vector<FidoDevice*> devices;
  devices.reserve(devices_.size());
  for (const auto& device : devices_)
    devices.push_back(device.second.get());
  return devices;
}

std::vector<const FidoDevice*> FidoDiscovery::GetDevices() const {
  std::vector<const FidoDevice*> devices;
  devices.reserve(devices_.size());
  for (const auto& device : devices_)
    devices.push_back(device.second.get());
  return devices;
}

FidoDevice* FidoDiscovery::GetDevice(base::StringPiece device_id) {
  return const_cast<FidoDevice*>(
      static_cast<const FidoDiscovery*>(this)->GetDevice(device_id));
}

const FidoDevice* FidoDiscovery::GetDevice(base::StringPiece device_id) const {
  auto found = devices_.find(device_id);
  return found != devices_.end() ? found->second.get() : nullptr;
}

bool FidoDiscovery::AddDevice(std::unique_ptr<FidoDevice> device) {
  std::string device_id = device->GetId();
  const auto result = devices_.emplace(std::move(device_id), std::move(device));
  if (result.second)
    NotifyDeviceAdded(result.first->second.get());
  return result.second;
}

bool FidoDiscovery::RemoveDevice(base::StringPiece device_id) {
  auto found = devices_.find(device_id);
  if (found == devices_.end())
    return false;

  auto device = std::move(found->second);
  devices_.erase(found);
  NotifyDeviceRemoved(device.get());
  return true;
}

// ScopedFidoDiscoveryFactory -------------------------------------------------

namespace internal {

ScopedFidoDiscoveryFactory::ScopedFidoDiscoveryFactory() {
  original_factory_ = std::exchange(g_current_factory, this);
  original_factory_func_ =
      std::exchange(FidoDiscovery::g_factory_func_,
                    &ForwardCreateFidoDiscoveryToCurrentFactory);
}

ScopedFidoDiscoveryFactory::~ScopedFidoDiscoveryFactory() {
  g_current_factory = original_factory_;
  FidoDiscovery::g_factory_func_ = original_factory_func_;
}

// static
std::unique_ptr<FidoDiscovery>
ScopedFidoDiscoveryFactory::ForwardCreateFidoDiscoveryToCurrentFactory(
    U2fTransportProtocol transport,
    ::service_manager::Connector* connector) {
  DCHECK(g_current_factory);
  return g_current_factory->CreateFidoDiscovery(transport, connector);
}

// static
ScopedFidoDiscoveryFactory* ScopedFidoDiscoveryFactory::g_current_factory =
    nullptr;

}  // namespace internal
}  // namespace device
