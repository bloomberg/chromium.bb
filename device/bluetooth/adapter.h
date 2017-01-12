// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_ADAPTER_H_
#define DEVICE_BLUETOOTH_ADAPTER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"
#include "device/bluetooth/public/interfaces/adapter.mojom.h"
#include "device/bluetooth/public/interfaces/device.mojom.h"

namespace bluetooth {

// Implementation of Mojo Adapter located in
// device/bluetooth/public/interfaces/adapter.mojom.
// It handles requests for Bluetooth adapter capabilities
// and devices and uses the platform abstraction of device/bluetooth.
class Adapter : public mojom::Adapter,
                public device::BluetoothAdapter::Observer {
 public:
  explicit Adapter(scoped_refptr<device::BluetoothAdapter> adapter);
  ~Adapter() override;

  // mojom::Adapter overrides:
  void ConnectToDevice(const std::string& address,
                       const ConnectToDeviceCallback& callback) override;
  void GetDevices(const GetDevicesCallback& callback) override;
  void GetInfo(const GetInfoCallback& callback) override;
  void SetClient(mojom::AdapterClientPtr client) override;
  void StartDiscoverySession(
      const StartDiscoverySessionCallback& callback) override;

  // device::BluetoothAdapter::Observer overrides:
  void AdapterPresentChanged(device::BluetoothAdapter* adapter,
                             bool present) override;
  void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                             bool powered) override;
  void AdapterDiscoverableChanged(device::BluetoothAdapter* adapter,
                                  bool discoverable) override;
  void AdapterDiscoveringChanged(device::BluetoothAdapter* adapter,
                                 bool discovering) override;
  void DeviceAdded(device::BluetoothAdapter* adapter,
                   device::BluetoothDevice* device) override;
  void DeviceChanged(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;
  void DeviceRemoved(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;

 private:
  void OnGattConnected(
      const ConnectToDeviceCallback& callback,
      std::unique_ptr<device::BluetoothGattConnection> connection);

  void OnConnectError(const ConnectToDeviceCallback& callback,
                      device::BluetoothDevice::ConnectErrorCode error_code);

  void OnStartDiscoverySession(
      const StartDiscoverySessionCallback& callback,
      std::unique_ptr<device::BluetoothDiscoverySession> session);

  void OnDiscoverySessionError(const StartDiscoverySessionCallback& callback);

  // The current Bluetooth adapter.
  scoped_refptr<device::BluetoothAdapter> adapter_;

  // The adapter client that listens to this service.
  mojom::AdapterClientPtr client_;

  base::WeakPtrFactory<Adapter> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Adapter);
};

}  // namespace bluetooth

#endif  // DEVICE_BLUETOOTH_ADAPTER_H_
