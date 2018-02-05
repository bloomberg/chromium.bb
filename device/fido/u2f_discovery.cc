// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/u2f_discovery.h"

#include <utility>

#include "device/fido/u2f_device.h"

namespace device {

U2fDiscovery::U2fDiscovery() = default;

U2fDiscovery::~U2fDiscovery() = default;

void U2fDiscovery::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void U2fDiscovery::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void U2fDiscovery::NotifyDiscoveryStarted(bool success) {
  for (auto& observer : observers_)
    observer.DiscoveryStarted(this, success);
}

void U2fDiscovery::NotifyDiscoveryStopped(bool success) {
  for (auto& observer : observers_)
    observer.DiscoveryStopped(this, success);
}

void U2fDiscovery::NotifyDeviceAdded(U2fDevice* device) {
  for (auto& observer : observers_)
    observer.DeviceAdded(this, device);
}

void U2fDiscovery::NotifyDeviceRemoved(U2fDevice* device) {
  for (auto& observer : observers_)
    observer.DeviceRemoved(this, device);
}

std::vector<U2fDevice*> U2fDiscovery::GetDevices() {
  std::vector<U2fDevice*> devices;
  devices.reserve(devices_.size());
  for (const auto& device : devices_)
    devices.push_back(device.second.get());
  return devices;
}

std::vector<const U2fDevice*> U2fDiscovery::GetDevices() const {
  std::vector<const U2fDevice*> devices;
  devices.reserve(devices_.size());
  for (const auto& device : devices_)
    devices.push_back(device.second.get());
  return devices;
}

U2fDevice* U2fDiscovery::GetDevice(base::StringPiece device_id) {
  return const_cast<U2fDevice*>(
      static_cast<const U2fDiscovery*>(this)->GetDevice(device_id));
}

const U2fDevice* U2fDiscovery::GetDevice(base::StringPiece device_id) const {
  auto found = devices_.find(device_id);
  return found != devices_.end() ? found->second.get() : nullptr;
}

bool U2fDiscovery::AddDevice(std::unique_ptr<U2fDevice> device) {
  std::string device_id = device->GetId();
  const auto result = devices_.emplace(std::move(device_id), std::move(device));
  if (result.second)
    NotifyDeviceAdded(result.first->second.get());
  return result.second;
}

bool U2fDiscovery::RemoveDevice(base::StringPiece device_id) {
  auto found = devices_.find(device_id);
  if (found == devices_.end())
    return false;

  auto device = std::move(found->second);
  devices_.erase(found);
  NotifyDeviceRemoved(device.get());
  return true;
}

}  // namespace device
