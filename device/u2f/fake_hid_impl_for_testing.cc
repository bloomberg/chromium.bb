// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/u2f/fake_hid_impl_for_testing.h"

namespace device {

bool FakeHidConnection::mock_connection_error_ = false;

FakeHidConnection::FakeHidConnection(device::mojom::HidDeviceInfoPtr device)
    : device_(std::move(device)) {}

FakeHidConnection::~FakeHidConnection() {}

void FakeHidConnection::Read(ReadCallback callback) {
  std::vector<uint8_t> buffer = {'F', 'a', 'k', 'e', ' ', 'H', 'i', 'd'};
  std::move(callback).Run(true, 0, buffer);
}

void FakeHidConnection::Write(uint8_t report_id,
                              const std::vector<uint8_t>& buffer,
                              WriteCallback callback) {
  if (mock_connection_error_) {
    std::move(callback).Run(false);
    return;
  }

  std::move(callback).Run(true);
}

void FakeHidConnection::GetFeatureReport(uint8_t report_id,
                                         GetFeatureReportCallback callback) {
  NOTREACHED();
}

void FakeHidConnection::SendFeatureReport(uint8_t report_id,
                                          const std::vector<uint8_t>& buffer,
                                          SendFeatureReportCallback callback) {
  NOTREACHED();
}

FakeHidManager::FakeHidManager() {}

FakeHidManager::~FakeHidManager() {}

void FakeHidManager::AddBinding(mojo::ScopedMessagePipeHandle handle) {
  bindings_.AddBinding(this,
                       device::mojom::HidManagerRequest(std::move(handle)));
}

void FakeHidManager::AddBinding2(device::mojom::HidManagerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void FakeHidManager::GetDevicesAndSetClient(
    device::mojom::HidManagerClientAssociatedPtrInfo client,
    GetDevicesCallback callback) {
  GetDevices(std::move(callback));

  device::mojom::HidManagerClientAssociatedPtr client_ptr;
  client_ptr.Bind(std::move(client));
  clients_.AddPtr(std::move(client_ptr));
}

void FakeHidManager::GetDevices(GetDevicesCallback callback) {
  std::vector<device::mojom::HidDeviceInfoPtr> device_list;
  for (auto& map_entry : devices_)
    device_list.push_back(map_entry.second->Clone());

  std::move(callback).Run(std::move(device_list));
}

void FakeHidManager::Connect(const std::string& device_guid,
                             ConnectCallback callback) {
  auto it = devices_.find(device_guid);
  if (it == devices_.end()) {
    std::move(callback).Run(nullptr);
    return;
  }

  // Strongly binds an instance of FakeHidConnctionImpl.
  device::mojom::HidConnectionPtr client;
  mojo::MakeStrongBinding(
      base::MakeUnique<FakeHidConnection>(it->second->Clone()),
      mojo::MakeRequest(&client));
  std::move(callback).Run(std::move(client));
}

void FakeHidManager::AddDevice(device::mojom::HidDeviceInfoPtr device) {
  device::mojom::HidDeviceInfo* device_info = device.get();
  clients_.ForAllPtrs([device_info](device::mojom::HidManagerClient* client) {
    client->DeviceAdded(device_info->Clone());
  });

  devices_[device->guid] = std::move(device);
}

void FakeHidManager::RemoveDevice(const std::string device_guid) {
  auto it = devices_.find(device_guid);
  if (it == devices_.end())
    return;

  device::mojom::HidDeviceInfo* device_info = it->second.get();
  clients_.ForAllPtrs([device_info](device::mojom::HidManagerClient* client) {
    client->DeviceRemoved(device_info->Clone());
  });
  devices_.erase(it);
}

}  // namespace device
