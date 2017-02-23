// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_MAC_H_
#define DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_MAC_H_

#include <memory>

#include "base/test/test_simple_task_runner.h"
#include "device/bluetooth/test/bluetooth_test.h"

#if __OBJC__
@class MockCBCharacteristic;
@class MockCBPeripheral;
#else   // __OBJC__
class MockCBCharacteristic;
class MockCBPeripheral;
#endif  // __OBJC__

namespace device {

class BluetoothAdapterMac;

// Mac implementation of BluetoothTestBase.
class BluetoothTestMac : public BluetoothTestBase {
 public:
  static const std::string kTestPeripheralUUID1;
  static const std::string kTestPeripheralUUID2;

  BluetoothTestMac();
  ~BluetoothTestMac() override;

  // Test overrides:
  void SetUp() override;

  // BluetoothTestBase overrides:
  bool PlatformSupportsLowEnergy() override;
  void InitWithDefaultAdapter() override;
  void InitWithoutDefaultAdapter() override;
  void InitWithFakeAdapter() override;
  void ResetEventCounts() override;
  BluetoothDevice* SimulateLowEnergyDevice(int device_ordinal) override;
  void SimulateConnectedLowEnergyDevice(
      ConnectedDeviceType device_ordinal) override;
  void SimulateGattConnection(BluetoothDevice* device) override;
  void SimulateGattConnectionError(
      BluetoothDevice* device,
      BluetoothDevice::ConnectErrorCode errorCode) override;
  void SimulateGattDisconnection(BluetoothDevice* device) override;
  void SimulateGattServicesDiscovered(
      BluetoothDevice* device,
      const std::vector<std::string>& uuids) override;
  void SimulateGattServicesChanged(BluetoothDevice* device) override;
  void SimulateGattServiceRemoved(BluetoothRemoteGattService* service) override;
  void SimulateGattCharacteristic(BluetoothRemoteGattService* service,
                                  const std::string& uuid,
                                  int properties) override;
  void SimulateGattCharacteristicRead(
      BluetoothRemoteGattCharacteristic* characteristic,
      const std::vector<uint8_t>& value) override;
  void SimulateGattCharacteristicReadError(
      BluetoothRemoteGattCharacteristic* characteristic,
      BluetoothRemoteGattService::GattErrorCode) override;
  void SimulateGattCharacteristicWrite(
      BluetoothRemoteGattCharacteristic* characteristic) override;
  void SimulateGattCharacteristicWriteError(
      BluetoothRemoteGattCharacteristic* characteristic,
      BluetoothRemoteGattService::GattErrorCode error_code) override;
  void SimulateGattDescriptor(BluetoothRemoteGattCharacteristic* characteristic,
                              const std::string& uuid) override;
  void SimulateGattNotifySessionStarted(
      BluetoothRemoteGattCharacteristic* characteristic) override;
  void SimulateGattNotifySessionStartError(
      BluetoothRemoteGattCharacteristic* characteristic,
      BluetoothRemoteGattService::GattErrorCode error_code) override;
  void SimulateGattNotifySessionStopped(
      BluetoothRemoteGattCharacteristic* characteristic) override;
  void SimulateGattNotifySessionStopError(
      BluetoothRemoteGattCharacteristic* characteristic,
      BluetoothRemoteGattService::GattErrorCode error_code) override;
  void SimulateGattCharacteristicChanged(
      BluetoothRemoteGattCharacteristic* characteristic,
      const std::vector<uint8_t>& value) override;
  void SimulateGattCharacteristicRemoved(
      BluetoothRemoteGattService* service,
      BluetoothRemoteGattCharacteristic* characteristic) override;
  void ExpectedChangeNotifyValueAttempts(int attempts) override;
  void ExpectedNotifyValue(NotifyValueState expected_value_state) override;

  // Callback for the bluetooth central manager mock.
  void OnFakeBluetoothDeviceConnectGattCalled();
  void OnFakeBluetoothGattDisconnect();

  // Callback for the bluetooth peripheral mock.
  void OnFakeBluetoothServiceDiscovery();
  void OnFakeBluetoothCharacteristicReadValue();
  void OnFakeBluetoothCharacteristicWriteValue(std::vector<uint8_t> value);
  void OnFakeBluetoothGattSetCharacteristicNotification(bool notify_value);

  // Returns the service UUIDs used to retrieve connected peripherals.
  BluetoothDevice::UUIDSet RetrieveConnectedPeripheralServiceUUIDs();
  // Reset RetrieveConnectedPeripheralServiceUUIDs set.
  void ResetRetrieveConnectedPeripheralServiceUUIDs();

 protected:
  class ScopedMockCentralManager;

  // Returns MockCBPeripheral from BluetoothRemoteGattService.
  MockCBPeripheral* GetMockCBPeripheral(
      BluetoothRemoteGattService* service) const;
  // Returns MockCBPeripheral from BluetoothRemoteGattCharacteristic.
  MockCBPeripheral* GetMockCBPeripheral(
      BluetoothRemoteGattCharacteristic* characteristic) const;
  // Returns MockCBCharacteristic from BluetoothRemoteGattCharacteristic.
  MockCBCharacteristic* GetCBMockCharacteristic(
      BluetoothRemoteGattCharacteristic* characteristic) const;

  // Utility function for finding CBUUIDs with relatively nice SHA256 hashes.
  std::string FindCBUUIDForHashTarget();

  BluetoothAdapterMac* adapter_mac_ = nullptr;
  std::unique_ptr<ScopedMockCentralManager> mock_central_manager_;

  // Value set by -[CBPeripheral setNotifyValue:forCharacteristic:] call.
  bool last_notify_value_ = false;
};

// Defines common test fixture name. Use TEST_F(BluetoothTest, YourTestName).
typedef BluetoothTestMac BluetoothTest;

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_MAC_H_
