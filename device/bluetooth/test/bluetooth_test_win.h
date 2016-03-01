// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_WIN_H_
#define DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_WIN_H_

#include "device/bluetooth/test/bluetooth_test.h"

#include "base/memory/ref_counted.h"
#include "base/test/test_pending_task.h"
#include "base/test/test_simple_task_runner.h"
#include "device/bluetooth/bluetooth_classic_win_fake.h"
#include "device/bluetooth/bluetooth_low_energy_win_fake.h"

namespace device {
class BluetoothAdapterWin;

// Windows implementation of BluetoothTestBase.
class BluetoothTestWin : public BluetoothTestBase {
 public:
  BluetoothTestWin();
  ~BluetoothTestWin() override;

  // BluetoothTestBase overrides
  bool PlatformSupportsLowEnergy() override;
  void InitWithDefaultAdapter() override;
  void InitWithoutDefaultAdapter() override;
  void InitWithFakeAdapter() override;
  bool DenyPermission() override;
  void StartLowEnergyDiscoverySession() override;
  BluetoothDevice* DiscoverLowEnergyDevice(int device_ordinal) override;
  void SimulateGattConnection(BluetoothDevice* device) override;
  void SimulateGattServicesDiscovered(
      BluetoothDevice* device,
      const std::vector<std::string>& uuids) override;
  void SimulateGattServiceRemoved(BluetoothGattService* service) override;
  void SimulateGattCharacteristic(BluetoothGattService* service,
                                  const std::string& uuid,
                                  int properties) override;
  void SimulateGattCharacteristicRemoved(
      BluetoothGattService* service,
      BluetoothGattCharacteristic* characteristic) override;
  void SimulateGattDescriptor(BluetoothGattCharacteristic* characteristic,
                              const std::string& uuid) override;

 private:
  scoped_refptr<base::TestSimpleTaskRunner> ui_task_runner_;
  scoped_refptr<base::TestSimpleTaskRunner> bluetooth_task_runner_;
  BluetoothAdapterWin* adapter_win_;

  win::BluetoothClassicWrapperFake* fake_bt_classic_wrapper_;
  win::BluetoothLowEnergyWrapperFake* fake_bt_le_wrapper_;

  void AdapterInitCallback();
  win::GattService* GetSimulatedService(win::BLEDevice* device,
                                        BluetoothGattService* service);
  win::GattCharacteristic* GetSimulatedCharacteristic(
      BluetoothGattCharacteristic* characteristic);
  void ForceRefreshDevice();
  void FinishPendingTasks();
};

// Defines common test fixture name. Use TEST_F(BluetoothTest, YourTestName).
typedef BluetoothTestWin BluetoothTest;

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_WIN_H_
