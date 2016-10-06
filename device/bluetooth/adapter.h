// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_ADAPTER_H_
#define DEVICE_BLUETOOTH_ADAPTER_H_

#include "base/macros.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/public/interfaces/adapter.mojom.h"

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
  void GetDevices(const GetDevicesCallback& callback) override;
  void SetClient(mojom::AdapterClientPtr client) override;

  // device::BluetoothAdapter::Observer overrides:
  void DeviceAdded(device::BluetoothAdapter* adapter,
                   device::BluetoothDevice* device) override;
  void DeviceRemoved(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;

 private:
  // Creates a mojom::DeviceInfo using info from the given |device|.
  static mojom::DeviceInfoPtr ConstructDeviceInfoStruct(
      const device::BluetoothDevice* device);

  // The current Bluetooth adapter.
  scoped_refptr<device::BluetoothAdapter> adapter_;

  // The adapter client that listens to this service.
  mojom::AdapterClientPtr client_;

  base::WeakPtrFactory<Adapter> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Adapter);
};

}  // namespace bluetooth

#endif  // DEVICE_BLUETOOTH_ADAPTER_H_
