// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_WIN_FAKE_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_WIN_FAKE_H_

#include "device/bluetooth/bluetooth_low_energy_win.h"

#include <set>
#include <unordered_map>

namespace device {
namespace win {

struct BLEDevice;
struct GattService;
struct GattCharacteristic;
struct GattDescriptor;

// The key of BLEDevicesMap is the string of the BLE device address.
typedef std::unordered_map<std::string, scoped_ptr<BLEDevice>> BLEDevicesMap;
// The key of GattServicesMap, GattCharacteristicsMap and GattDescriptorsMap is
// the string of the attribute handle.
typedef std::unordered_map<std::string, scoped_ptr<GattService>>
    GattServicesMap;
typedef std::unordered_map<std::string, scoped_ptr<GattCharacteristic>>
    GattCharacteristicsMap;
typedef std::unordered_map<std::string, scoped_ptr<GattDescriptor>>
    GattDescriptorsMap;
// The key of BLEAttributeHandleTable is the string of the BLE device address.
typedef std::unordered_map<std::string, scoped_ptr<std::set<USHORT>>>
    BLEAttributeHandleTable;

struct BLEDevice {
  BLEDevice();
  ~BLEDevice();
  scoped_ptr<BluetoothLowEnergyDeviceInfo> device_info;
  GattServicesMap primary_services;
};

struct GattService {
  GattService();
  ~GattService();
  scoped_ptr<BTH_LE_GATT_SERVICE> service_info;
  GattServicesMap included_services;
  GattCharacteristicsMap included_characteristics;
};

struct GattCharacteristic {
  GattCharacteristic();
  ~GattCharacteristic();
  scoped_ptr<BTH_LE_GATT_CHARACTERISTIC> characteristic_info;
  scoped_ptr<BTH_LE_GATT_CHARACTERISTIC_VALUE> value;
  GattDescriptorsMap included_descriptors;
};

struct GattDescriptor {
  GattDescriptor();
  ~GattDescriptor();
  scoped_ptr<BTH_LE_GATT_DESCRIPTOR> descriptor_info;
  scoped_ptr<BTH_LE_GATT_DESCRIPTOR_VALUE> value;
};

// Fake implementation of BluetoothLowEnergyWrapper. Used for BluetoothTestWin.
class BluetoothLowEnergyWrapperFake : public BluetoothLowEnergyWrapper {
 public:
  BluetoothLowEnergyWrapperFake();
  ~BluetoothLowEnergyWrapperFake() override;

  bool IsBluetoothLowEnergySupported() override;
  bool EnumerateKnownBluetoothLowEnergyDevices(
      ScopedVector<BluetoothLowEnergyDeviceInfo>* devices,
      std::string* error) override;
  bool EnumerateKnownBluetoothLowEnergyGattServiceDevices(
      ScopedVector<BluetoothLowEnergyDeviceInfo>* devices,
      std::string* error) override;
  bool EnumerateKnownBluetoothLowEnergyServices(
      const base::FilePath& device_path,
      ScopedVector<BluetoothLowEnergyServiceInfo>* services,
      std::string* error) override;
  HRESULT ReadCharacteristicsOfAService(
      base::FilePath& service_path,
      const PBTH_LE_GATT_SERVICE service,
      scoped_ptr<BTH_LE_GATT_CHARACTERISTIC>* out_included_characteristics,
      USHORT* out_counts) override;
  HRESULT ReadDescriptorsOfACharacteristic(
      base::FilePath& service_path,
      const PBTH_LE_GATT_CHARACTERISTIC characteristic,
      scoped_ptr<BTH_LE_GATT_DESCRIPTOR>* out_included_descriptors,
      USHORT* out_counts) override;

  BLEDevice* SimulateBLEDevice(std::string device_name,
                               BLUETOOTH_ADDRESS device_address);
  BLEDevice* GetSimulatedBLEDevice(std::string device_address);

  // Note: |parent_service| may be nullptr to indicate a primary service.
  GattService* SimulateGattService(BLEDevice* device,
                                   GattService* parent_service,
                                   const BTH_LE_UUID& uuid);

  // Note: |parent_service| may be nullptr to indicate a primary service.
  void SimulateGattServiceRemoved(BLEDevice* device,
                                  GattService* parent_service,
                                  std::string attribute_handle);

  // Note: |chain_of_att_handle| contains the attribute handles of the services
  // in order from primary service to target service. The last item in
  // |chain_of_att_handle| is the target service's attribute handle.
  GattService* GetSimulatedGattService(
      BLEDevice* device,
      const std::vector<std::string>& chain_of_att_handle);
  GattCharacteristic* SimulateGattCharacterisc(
      std::string device_address,
      GattService* parent_service,
      const BTH_LE_GATT_CHARACTERISTIC& characteristic);
  void SimulateGattCharacteriscRemove(GattService* parent_service,
                                      std::string attribute_handle);
  GattCharacteristic* GetSimulatedGattCharacteristic(
      GattService* parent_service,
      std::string attribute_handle);
  void SimulateGattDescriptor(std::string device_address,
                              GattCharacteristic* characteristic,
                              const BTH_LE_UUID& uuid);

 private:
  // Get simulated characteristic by |service_path| and |characteristic| info.
  GattCharacteristic* GetSimulatedGattCharacteristic(
      base::FilePath& service_path,
      const PBTH_LE_GATT_CHARACTERISTIC characteristic);

  // Generate an unique attribute handle on |device_address|.
  USHORT GenerateAUniqueAttributeHandle(std::string device_address);

  // Generate device path for the BLE device with |device_address|.
  base::string16 GenerateBLEDevicePath(std::string device_address);

  // Generate GATT service device path of the service with
  // |service_attribute_handle|. |resident_device_path| is the BLE device this
  // GATT service belongs to.
  base::string16 GenerateGattServiceDevicePath(
      base::string16 resident_device_path,
      USHORT service_attribute_handle);

  // Extract device address from the device |path| generated by
  // GenerateBLEDevicePath or GenerateGattServiceDevicePath.
  base::string16 ExtractDeviceAddressFromDevicePath(base::string16 path);

  // Extract service attribute handles from the |path| generated by
  // GenerateGattServiceDevicePath.
  std::vector<std::string> ExtractServiceAttributeHandlesFromDevicePath(
      base::string16 path);

  // The canonical BLE device address string format is the
  // BluetoothDevice::CanonicalizeAddress.
  std::string BluetoothAddressToCanonicalString(const BLUETOOTH_ADDRESS& btha);

  // Table to store allocated attribute handle for a device.
  BLEAttributeHandleTable attribute_handle_table_;
  BLEDevicesMap simulated_devices_;
};

}  // namespace win
}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_WIN_FAKE_H_
