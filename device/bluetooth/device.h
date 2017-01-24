// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DEVICE_H_
#define DEVICE_BLUETOOTH_DEVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"
#include "device/bluetooth/public/interfaces/device.mojom.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace bluetooth {

// Implementation of Mojo Device located in
// device/bluetooth/public/interfaces/device.mojom.
// It handles requests to interact with Bluetooth Device.
// Uses the platform abstraction of device/bluetooth.
// An instance of this class is constructed by Adapter and strongly bound
// to its MessagePipe. In the case where the BluetoothGattConnection dies, the
// instance closes the binding which causes the instance to be deleted.
class Device : public mojom::Device, public device::BluetoothAdapter::Observer {
 public:
  ~Device() override;

  static void Create(
      scoped_refptr<device::BluetoothAdapter> adapter,
      std::unique_ptr<device::BluetoothGattConnection> connection,
      mojom::DeviceRequest request);

  // Creates a mojom::DeviceInfo using info from the given |device|.
  static mojom::DeviceInfoPtr ConstructDeviceInfoStruct(
      const device::BluetoothDevice* device);

  // BluetoothAdapter::Observer overrides:
  void DeviceChanged(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;
  void GattServicesDiscovered(device::BluetoothAdapter* adapter,
                              device::BluetoothDevice* device) override;

  // mojom::Device overrides:
  void Disconnect() override;
  void GetInfo(const GetInfoCallback& callback) override;
  void GetServices(const GetServicesCallback& callback) override;
  void GetCharacteristics(const std::string& service_id,
                          const GetCharacteristicsCallback& callback) override;

 private:
  Device(scoped_refptr<device::BluetoothAdapter> adapter,
         std::unique_ptr<device::BluetoothGattConnection> connection);

  void GetServicesImpl(const GetServicesCallback& callback);

  mojom::ServiceInfoPtr ConstructServiceInfoStruct(
      const device::BluetoothRemoteGattService& service);

  const std::string& GetAddress();

  // The current BluetoothAdapter.
  scoped_refptr<device::BluetoothAdapter> adapter_;

  // The GATT connection to this device.
  std::unique_ptr<device::BluetoothGattConnection> connection_;

  mojo::StrongBindingPtr<mojom::Device> binding_;

  // The services request queue which holds callbacks that are waiting for
  // services to be discovered for this device.
  std::vector<base::Closure> pending_services_requests_;

  DISALLOW_COPY_AND_ASSIGN(Device);
};

}  // namespace bluetooth

#endif  // DEVICE_BLUETOOTH_DEVICE_H_
