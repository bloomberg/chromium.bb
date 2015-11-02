// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_BLUETOOTH_ADAPTER_OBSERVER_H_
#define DEVICE_BLUETOOTH_TEST_BLUETOOTH_ADAPTER_OBSERVER_H_

#include "device/bluetooth/bluetooth_adapter.h"

namespace device {

// Test implementation of BluetoothAdapter::Observer counting method calls and
// caching last reported values.
class TestBluetoothAdapterObserver : public BluetoothAdapter::Observer {
 public:
  TestBluetoothAdapterObserver(scoped_refptr<BluetoothAdapter> adapter);
  ~TestBluetoothAdapterObserver() override;

  // Reset counters and cached values.
  void Reset();

  // BluetoothAdapter::Observer
  void AdapterPresentChanged(BluetoothAdapter* adapter, bool present) override;
  void AdapterPoweredChanged(BluetoothAdapter* adapter, bool powered) override;
  void AdapterDiscoverableChanged(BluetoothAdapter* adapter,
                                  bool discoverable) override;
  void AdapterDiscoveringChanged(BluetoothAdapter* adapter,
                                 bool discovering) override;
  void DeviceAdded(BluetoothAdapter* adapter, BluetoothDevice* device) override;
  void DeviceChanged(BluetoothAdapter* adapter,
                     BluetoothDevice* device) override;
  void DeviceAddressChanged(device::BluetoothAdapter* adapter,
                            device::BluetoothDevice* device,
                            const std::string& old_address) override;
  void DeviceRemoved(BluetoothAdapter* adapter,
                     BluetoothDevice* device) override;
  void GattServiceAdded(BluetoothAdapter* adapter,
                        BluetoothDevice* device,
                        BluetoothGattService* service) override;
  void GattServiceRemoved(BluetoothAdapter* adapter,
                          BluetoothDevice* device,
                          BluetoothGattService* service) override;
  void GattServicesDiscovered(BluetoothAdapter* adapter,
                              BluetoothDevice* device) override;
  void GattDiscoveryCompleteForService(BluetoothAdapter* adapter,
                                       BluetoothGattService* service) override;
  void GattServiceChanged(BluetoothAdapter* adapter,
                          BluetoothGattService* service) override;
  void GattCharacteristicAdded(
      BluetoothAdapter* adapter,
      BluetoothGattCharacteristic* characteristic) override;
  void GattCharacteristicRemoved(
      BluetoothAdapter* adapter,
      BluetoothGattCharacteristic* characteristic) override;
  void GattDescriptorAdded(BluetoothAdapter* adapter,
                           BluetoothGattDescriptor* descriptor) override;
  void GattDescriptorRemoved(BluetoothAdapter* adapter,
                             BluetoothGattDescriptor* descriptor) override;
  void GattCharacteristicValueChanged(
      BluetoothAdapter* adapter,
      BluetoothGattCharacteristic* characteristic,
      const std::vector<uint8>& value) override;
  void GattDescriptorValueChanged(BluetoothAdapter* adapter,
                                  BluetoothGattDescriptor* descriptor,
                                  const std::vector<uint8>& value) override;

  // Adapter related:
  int present_changed_count() { return present_changed_count_; }
  int powered_changed_count() { return powered_changed_count_; }
  int discoverable_changed_count() { return discoverable_changed_count_; }
  int discovering_changed_count() { return discovering_changed_count_; }
  bool last_present() { return last_present_; }
  bool last_powered() { return last_powered_; }
  bool last_discovering() { return last_discovering_; }

  // Device related:
  int device_added_count() { return device_added_count_; }
  int device_changed_count() { return device_changed_count_; }
  int device_address_changed_count() { return device_address_changed_count_; }
  int device_removed_count() { return device_removed_count_; }
  BluetoothDevice* last_device() { return last_device_; }
  std::string last_device_address() { return last_device_address_; }

  // GATT related:
  int gatt_service_added_count() { return gatt_service_added_count_; }
  int gatt_service_removed_count() { return gatt_service_removed_count_; }
  int gatt_services_discovered_count() {
    return gatt_services_discovered_count_;
  }
  int gatt_service_changed_count() { return gatt_service_changed_count_; }
  int gatt_discovery_complete_count() { return gatt_discovery_complete_count_; }
  int gatt_characteristic_added_count() {
    return gatt_characteristic_added_count_;
  }
  int gatt_characteristic_removed_count() {
    return gatt_characteristic_removed_count_;
  }
  int gatt_characteristic_value_changed_count() {
    return gatt_characteristic_value_changed_count_;
  }
  int gatt_descriptor_added_count() { return gatt_descriptor_added_count_; }
  int gatt_descriptor_removed_count() { return gatt_descriptor_removed_count_; }
  int gatt_descriptor_value_changed_count() {
    return gatt_descriptor_value_changed_count_;
  }
  std::string last_gatt_service_id() { return last_gatt_service_id_; }
  BluetoothUUID last_gatt_service_uuid() { return last_gatt_service_uuid_; }
  std::string last_gatt_characteristic_id() {
    return last_gatt_characteristic_id_;
  }
  BluetoothUUID last_gatt_characteristic_uuid() {
    return last_gatt_characteristic_uuid_;
  }
  std::vector<uint8> last_changed_characteristic_value() {
    return last_changed_characteristic_value_;
  }
  std::string last_gatt_descriptor_id() { return last_gatt_descriptor_id_; }
  BluetoothUUID last_gatt_descriptor_uuid() {
    return last_gatt_descriptor_uuid_;
  }
  std::vector<uint8> last_changed_descriptor_value() {
    return last_changed_descriptor_value_;
  }

 private:
  // Some tests use a message loop since background processing is simulated;
  // break out of those loops.
  void QuitMessageLoop();

  scoped_refptr<BluetoothAdapter> adapter_;

  // Adapter related:
  int present_changed_count_;
  int powered_changed_count_;
  int discoverable_changed_count_;
  int discovering_changed_count_;
  bool last_present_;
  bool last_powered_;
  bool last_discovering_;

  // Device related:
  int device_added_count_;
  int device_changed_count_;
  int device_address_changed_count_;
  int device_removed_count_;
  BluetoothDevice* last_device_;
  std::string last_device_address_;

  // GATT related:
  int gatt_service_added_count_;
  int gatt_service_removed_count_;
  int gatt_services_discovered_count_;
  int gatt_service_changed_count_;
  int gatt_discovery_complete_count_;
  int gatt_characteristic_added_count_;
  int gatt_characteristic_removed_count_;
  int gatt_characteristic_value_changed_count_;
  int gatt_descriptor_added_count_;
  int gatt_descriptor_removed_count_;
  int gatt_descriptor_value_changed_count_;
  std::string last_gatt_service_id_;
  BluetoothUUID last_gatt_service_uuid_;
  std::string last_gatt_characteristic_id_;
  BluetoothUUID last_gatt_characteristic_uuid_;
  std::vector<uint8> last_changed_characteristic_value_;
  std::string last_gatt_descriptor_id_;
  BluetoothUUID last_gatt_descriptor_uuid_;
  std::vector<uint8> last_changed_descriptor_value_;

  DISALLOW_COPY_AND_ASSIGN(TestBluetoothAdapterObserver);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_BLUETOOTH_ADAPTER_OBSERVER_H_
