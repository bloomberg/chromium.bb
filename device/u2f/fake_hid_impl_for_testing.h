// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "device/hid/public/interfaces/hid.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace device {

class FakeHidConnection : public device::mojom::HidConnection {
 public:
  explicit FakeHidConnection(device::mojom::HidDeviceInfoPtr device);

  ~FakeHidConnection() override;

  // device::mojom::HidConnection implemenation:
  void Read(ReadCallback callback) override;
  void Write(uint8_t report_id,
             const std::vector<uint8_t>& buffer,
             WriteCallback callback) override;
  void GetFeatureReport(uint8_t report_id,
                        GetFeatureReportCallback callback) override;
  void SendFeatureReport(uint8_t report_id,
                         const std::vector<uint8_t>& buffer,
                         SendFeatureReportCallback callback) override;

  static bool mock_connection_error_;

 private:
  device::mojom::HidDeviceInfoPtr device_;
};

class FakeHidManager : public device::mojom::HidManager {
 public:
  FakeHidManager();
  ~FakeHidManager() override;

  // device::mojom::HidManager implementation:
  void GetDevicesAndSetClient(
      device::mojom::HidManagerClientAssociatedPtrInfo client,
      GetDevicesCallback callback) override;
  void GetDevices(GetDevicesCallback callback) override;
  void Connect(const std::string& device_guid,
               ConnectCallback callback) override;
  void AddBinding(mojo::ScopedMessagePipeHandle handle);
  void AddBinding2(device::mojom::HidManagerRequest request);
  void AddDevice(device::mojom::HidDeviceInfoPtr device);
  void RemoveDevice(const std::string device_guid);

 private:
  std::map<std::string, device::mojom::HidDeviceInfoPtr> devices_;
  mojo::AssociatedInterfacePtrSet<device::mojom::HidManagerClient> clients_;
  mojo::BindingSet<device::mojom::HidManager> bindings_;
};

}  // namespace device
