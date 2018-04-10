// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_CAST_H_
#define DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_CAST_H_

#include <cstdint>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/optional.h"
#include "chromecast/device/bluetooth/shlib/mock_gatt_client.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_local_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_local_gatt_descriptor.h"
#include "device/bluetooth/test/bluetooth_test.h"

namespace chromecast {
namespace bluetooth {
class GattClientManagerImpl;
}
}  // namespace chromecast

namespace device {

// Cast implementation of BluetoothTestBase.
class BluetoothTestCast : public BluetoothTestBase {
 public:
  BluetoothTestCast();
  ~BluetoothTestCast() override;

  // BluetoothTestBase overrides:
  bool PlatformSupportsLowEnergy() override;
  void InitWithFakeAdapter() override;
  BluetoothDevice* SimulateLowEnergyDevice(int device_ordinal) override;

 private:
  class FakeGattClientManager;
  class FakeLeScanManager;

  void UpdateAdapter(
      const std::string& address,
      const base::Optional<std::string>& name,
      const std::vector<std::string>& service_uuids,
      const std::map<std::string, std::vector<uint8_t>>& service_data,
      const std::map<uint16_t, std::vector<uint8_t>>& manufacturer_data);

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  chromecast::bluetooth_v2_shlib::MockGattClient mock_gatt_client_;
  std::unique_ptr<chromecast::bluetooth::GattClientManagerImpl>
      gatt_client_manager_;
  std::unique_ptr<FakeLeScanManager> fake_le_scan_manager_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothTestCast);
};

// Defines common test fixture name. Use TEST_F(BluetoothTest, YourTestName).
using BluetoothTest = BluetoothTestCast;

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_CAST_H_
