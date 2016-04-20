// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_BLUETOOTH_ARC_BLUETOOTH_BRIDGE_H_
#define COMPONENTS_ARC_BLUETOOTH_ARC_BLUETOOTH_BRIDGE_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service.h"
#include "components/arc/common/bluetooth.mojom.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace arc {

class ArcBridgeService;

class ArcBluetoothBridge
    : public ArcService,
      public ArcBridgeService::Observer,
      public device::BluetoothAdapter::Observer,
      public device::BluetoothAdapterFactory::AdapterCallback,
      public mojom::BluetoothHost {
 public:
  explicit ArcBluetoothBridge(ArcBridgeService* bridge_service);
  ~ArcBluetoothBridge() override;

  // Overridden from ArcBridgeService::Observer:
  void OnBluetoothInstanceReady() override;

  void OnAdapterInitialized(scoped_refptr<device::BluetoothAdapter> adapter);

  // Overridden from device::BluetoothAdadpter::Observer
  void AdapterPresentChanged(device::BluetoothAdapter* adapter,
                             bool present) override;

  void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                             bool powered) override;

  void DeviceAdded(device::BluetoothAdapter* adapter,
                   device::BluetoothDevice* device) override;

  void DeviceChanged(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;

  void DeviceAddressChanged(device::BluetoothAdapter* adapter,
                            device::BluetoothDevice* device,
                            const std::string& old_address) override;

  void DevicePairedChanged(device::BluetoothAdapter* adapter,
                           device::BluetoothDevice* device,
                           bool new_paired_status) override;

  void DeviceRemoved(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;

  void GattServiceAdded(device::BluetoothAdapter* adapter,
                        device::BluetoothDevice* device,
                        device::BluetoothRemoteGattService* service) override;

  void GattServiceRemoved(device::BluetoothAdapter* adapter,
                          device::BluetoothDevice* device,
                          device::BluetoothRemoteGattService* service) override;

  void GattServicesDiscovered(device::BluetoothAdapter* adapter,
                              device::BluetoothDevice* device) override;

  void GattDiscoveryCompleteForService(
      device::BluetoothAdapter* adapter,
      device::BluetoothRemoteGattService* service) override;

  void GattServiceChanged(device::BluetoothAdapter* adapter,
                          device::BluetoothRemoteGattService* service) override;

  void GattCharacteristicAdded(
      device::BluetoothAdapter* adapter,
      device::BluetoothRemoteGattCharacteristic* characteristic) override;

  void GattCharacteristicRemoved(
      device::BluetoothAdapter* adapter,
      device::BluetoothRemoteGattCharacteristic* characteristic) override;

  void GattDescriptorAdded(
      device::BluetoothAdapter* adapter,
      device::BluetoothRemoteGattDescriptor* descriptor) override;

  void GattDescriptorRemoved(
      device::BluetoothAdapter* adapter,
      device::BluetoothRemoteGattDescriptor* descriptor) override;

  void GattCharacteristicValueChanged(
      device::BluetoothAdapter* adapter,
      device::BluetoothRemoteGattCharacteristic* characteristic,
      const std::vector<uint8_t>& value) override;

  void GattDescriptorValueChanged(
      device::BluetoothAdapter* adapter,
      device::BluetoothRemoteGattDescriptor* descriptor,
      const std::vector<uint8_t>& value) override;

  // Bluetooth Mojo host interface
  void EnableAdapter(const EnableAdapterCallback& callback) override;
  void DisableAdapter(const DisableAdapterCallback& callback) override;

  void GetAdapterProperty(mojom::BluetoothPropertyType type) override;
  void SetAdapterProperty(mojom::BluetoothPropertyPtr property) override;

  void GetRemoteDeviceProperty(mojom::BluetoothAddressPtr remote_addr,
                               mojom::BluetoothPropertyType type) override;
  void SetRemoteDeviceProperty(mojom::BluetoothAddressPtr remote_addr,
                               mojom::BluetoothPropertyPtr property) override;
  void GetRemoteServiceRecord(mojom::BluetoothAddressPtr remote_addr,
                              mojom::BluetoothUUIDPtr uuid) override;

  void GetRemoteServices(mojom::BluetoothAddressPtr remote_addr) override;

  void StartDiscovery() override;
  void CancelDiscovery() override;

  void CreateBond(mojom::BluetoothAddressPtr addr, int32_t transport) override;
  void RemoveBond(mojom::BluetoothAddressPtr addr) override;
  void CancelBond(mojom::BluetoothAddressPtr addr) override;

  void GetConnectionState(mojom::BluetoothAddressPtr addr,
                          const GetConnectionStateCallback& callback) override;

  // Chrome observer callbacks
  void OnPoweredOn(
      const mojo::Callback<void(mojom::BluetoothAdapterState)>& callback) const;
  void OnPoweredOff(
      const mojo::Callback<void(mojom::BluetoothAdapterState)>& callback) const;
  void OnPoweredError(
      const mojo::Callback<void(mojom::BluetoothAdapterState)>& callback) const;
  void OnDiscoveryStarted(
      scoped_ptr<device::BluetoothDiscoverySession> session);
  void OnDiscoveryStopped();
  void OnDiscoveryError();
  void OnPairing(mojom::BluetoothAddressPtr addr) const;
  void OnPairedDone(mojom::BluetoothAddressPtr addr) const;
  void OnPairedError(
      mojom::BluetoothAddressPtr addr,
      device::BluetoothDevice::ConnectErrorCode error_code) const;
  void OnForgetDone(mojom::BluetoothAddressPtr addr) const;
  void OnForgetError(mojom::BluetoothAddressPtr addr) const;

 private:
  mojo::Array<mojom::BluetoothPropertyPtr> GetDeviceProperties(
      mojom::BluetoothPropertyType type,
      device::BluetoothDevice* device) const;
  mojo::Array<mojom::BluetoothPropertyPtr> GetAdapterProperties(
      mojom::BluetoothPropertyType type) const;

  void SendCachedDevicesFound() const;
  bool HasBluetoothInstance() const;

  // Propagates the list of paired device to Android.
  void SendCachedPairedDevices() const;

  mojo::Binding<mojom::BluetoothHost> binding_;

  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;
  scoped_ptr<device::BluetoothDiscoverySession> discovery_session_;

  // WeakPtrFactory to use for callbacks.
  base::WeakPtrFactory<ArcBluetoothBridge> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcBluetoothBridge);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_BLUETOOTH_ARC_BLUETOOTH_BRIDGE_H_
