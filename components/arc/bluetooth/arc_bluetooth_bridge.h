// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_BLUETOOTH_ARC_BLUETOOTH_BRIDGE_H_
#define COMPONENTS_ARC_BLUETOOTH_ARC_BLUETOOTH_BRIDGE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
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
#include "device/bluetooth/bluez/bluetooth_adapter_bluez.h"
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

  // Bluetooth Mojo host interface - Bluetooth Gatt Client functions
  void StartLEScan() override;
  void StopLEScan() override;
  void ConnectLEDevice(mojom::BluetoothAddressPtr remote_addr) override;
  void DisconnectLEDevice(mojom::BluetoothAddressPtr remote_addr) override;
  void StartLEListen(const StartLEListenCallback& callback) override;
  void StopLEListen(const StopLEListenCallback& callback) override;
  void SearchService(mojom::BluetoothAddressPtr remote_addr) override;

  int ConvertGattIdentifierToId(const std::string identifier) const;
  template <class T>
  mojom::BluetoothGattDBElementPtr CreateGattDBElement(
      const mojom::BluetoothGattDBAttributeType type,
      const T* GattObject) const;
  void GetGattDB(mojom::BluetoothAddressPtr remote_addr) override;
  void ReadGattCharacteristic(
      mojom::BluetoothAddressPtr remote_addr,
      mojom::BluetoothGattServiceIDPtr service_id,
      mojom::BluetoothGattIDPtr char_id,
      const ReadGattCharacteristicCallback& callback) override;
  void WriteGattCharacteristic(
      mojom::BluetoothAddressPtr remote_addr,
      mojom::BluetoothGattServiceIDPtr service_id,
      mojom::BluetoothGattIDPtr char_id,
      mojom::BluetoothGattValuePtr value,
      const WriteGattCharacteristicCallback& callback) override;
  void ReadGattDescriptor(mojom::BluetoothAddressPtr remote_addr,
                          mojom::BluetoothGattServiceIDPtr service_id,
                          mojom::BluetoothGattIDPtr char_id,
                          mojom::BluetoothGattIDPtr desc_id,
                          const ReadGattDescriptorCallback& callback) override;
  void WriteGattDescriptor(
      mojom::BluetoothAddressPtr remote_addr,
      mojom::BluetoothGattServiceIDPtr service_id,
      mojom::BluetoothGattIDPtr char_id,
      mojom::BluetoothGattIDPtr desc_id,
      mojom::BluetoothGattValuePtr value,
      const WriteGattDescriptorCallback& callback) override;
  void RegisterForGattNotification(
      mojom::BluetoothAddressPtr remote_addr,
      mojom::BluetoothGattServiceIDPtr service_id,
      mojom::BluetoothGattIDPtr char_id,
      const RegisterForGattNotificationCallback& callback) override;
  void DeregisterForGattNotification(
      mojom::BluetoothAddressPtr remote_addr,
      mojom::BluetoothGattServiceIDPtr service_id,
      mojom::BluetoothGattIDPtr char_id,
      const DeregisterForGattNotificationCallback& callback) override;
  void ReadRemoteRssi(mojom::BluetoothAddressPtr remote_addr,
                      const ReadRemoteRssiCallback& callback) override;

  // Chrome observer callbacks
  void OnPoweredOn(
      const base::Callback<void(mojom::BluetoothAdapterState)>& callback) const;
  void OnPoweredOff(
      const base::Callback<void(mojom::BluetoothAdapterState)>& callback) const;
  void OnPoweredError(
      const base::Callback<void(mojom::BluetoothAdapterState)>& callback) const;
  void OnDiscoveryStarted(
      std::unique_ptr<device::BluetoothDiscoverySession> session);
  void OnDiscoveryStopped();
  void OnDiscoveryError();
  void OnPairing(mojom::BluetoothAddressPtr addr) const;
  void OnPairedDone(mojom::BluetoothAddressPtr addr) const;
  void OnPairedError(
      mojom::BluetoothAddressPtr addr,
      device::BluetoothDevice::ConnectErrorCode error_code) const;
  void OnForgetDone(mojom::BluetoothAddressPtr addr) const;
  void OnForgetError(mojom::BluetoothAddressPtr addr) const;

  void OnGattConnectStateChanged(mojom::BluetoothAddressPtr addr,
                                 bool connected) const;
  void OnGattConnected(
      mojom::BluetoothAddressPtr addr,
      std::unique_ptr<device::BluetoothGattConnection> connection) const;
  void OnGattConnectError(
      mojom::BluetoothAddressPtr addr,
      device::BluetoothDevice::ConnectErrorCode error_code) const;
  void OnGattDisconnected(mojom::BluetoothAddressPtr addr) const;

  void OnStartLEListenDone(const StartLEListenCallback& callback,
                           scoped_refptr<device::BluetoothAdvertisement> adv);
  void OnStartLEListenError(
      const StartLEListenCallback& callback,
      device::BluetoothAdvertisement::ErrorCode error_code);

  void OnStopLEListenDone(const StopLEListenCallback& callback);
  void OnStopLEListenError(
      const StopLEListenCallback& callback,
      device::BluetoothAdvertisement::ErrorCode error_code);

  using GattReadCallback = base::Callback<void(mojom::BluetoothGattValuePtr)>;
  void OnGattReadDone(const GattReadCallback& callback,
                      const std::vector<uint8_t>& result) const;
  void OnGattReadError(
      const GattReadCallback& callback,
      device::BluetoothGattService::GattErrorCode error_code) const;

  using GattWriteCallback = base::Callback<void(mojom::BluetoothGattStatus)>;
  void OnGattWriteDone(const GattWriteCallback& callback) const;
  void OnGattWriteError(
      const GattWriteCallback& callback,
      device::BluetoothGattService::GattErrorCode error_code) const;

  void OnGattNotifyStartDone(
      const RegisterForGattNotificationCallback& callback,
      const std::string char_string_id,
      std::unique_ptr<device::BluetoothGattNotifySession> notify_session);
  void OnGattNotifyStartError(
      const RegisterForGattNotificationCallback& callback,
      device::BluetoothGattService::GattErrorCode error_code) const;
  void OnGattNotifyStopDone(
      const DeregisterForGattNotificationCallback& callback) const;

 private:
  mojo::Array<mojom::BluetoothPropertyPtr> GetDeviceProperties(
      mojom::BluetoothPropertyType type,
      device::BluetoothDevice* device) const;
  mojo::Array<mojom::BluetoothPropertyPtr> GetAdapterProperties(
      mojom::BluetoothPropertyType type) const;
  mojo::Array<mojom::BluetoothAdvertisingDataPtr> GetAdvertisingData(
      device::BluetoothDevice* device) const;

  void SendCachedDevicesFound() const;
  bool HasBluetoothInstance() const;
  bool CheckBluetoothInstanceVersion(int32_t version_need) const;

  template <class T>
  T* FindGattObjectFromUuid(const std::vector<T*> objs,
                            const device::BluetoothUUID uuid) const;
  device::BluetoothRemoteGattCharacteristic* FindGattCharacteristic(
      mojom::BluetoothAddressPtr remote_addr,
      mojom::BluetoothGattServiceIDPtr service_id,
      mojom::BluetoothGattIDPtr char_id) const;

  device::BluetoothRemoteGattDescriptor* FindGattDescriptor(
      mojom::BluetoothAddressPtr remote_addr,
      mojom::BluetoothGattServiceIDPtr service_id,
      mojom::BluetoothGattIDPtr char_id,
      mojom::BluetoothGattIDPtr desc_id) const;

  // Propagates the list of paired device to Android.
  void SendCachedPairedDevices() const;

  mojo::Binding<mojom::BluetoothHost> binding_;

  scoped_refptr<bluez::BluetoothAdapterBlueZ> bluetooth_adapter_;
  scoped_refptr<device::BluetoothAdvertisement> advertisment_;
  std::unique_ptr<device::BluetoothDiscoverySession> discovery_session_;
  std::map<std::string, std::unique_ptr<device::BluetoothGattNotifySession>>
      notification_session_;

  // WeakPtrFactory to use for callbacks.
  base::WeakPtrFactory<ArcBluetoothBridge> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcBluetoothBridge);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_BLUETOOTH_ARC_BLUETOOTH_BRIDGE_H_
