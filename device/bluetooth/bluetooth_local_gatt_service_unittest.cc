// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_local_gatt_characteristic.h"
#include "device/bluetooth/test/bluetooth_gatt_server_test.h"
#include "device/bluetooth/test/bluetooth_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class BluetoothLocalGattServiceTest : public BluetoothGattServerTest {
 public:
  void SetUp() override {
    BluetoothGattServerTest::SetUp();

    StartGattSetup();
    CompleteGattSetup();
  }

 protected:
  base::WeakPtr<BluetoothLocalGattCharacteristic> read_characteristic_;
  base::WeakPtr<BluetoothLocalGattCharacteristic> write_characteristic_;
};

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
TEST_F(BluetoothLocalGattServiceTest, RegisterMultipleServices) {
  base::WeakPtr<BluetoothLocalGattService> service2 =
      BluetoothLocalGattService::Create(
          adapter_.get(), BluetoothUUID(kTestUUIDGenericAttribute), true,
          nullptr, delegate_.get());
  base::WeakPtr<BluetoothLocalGattService> service3 =
      BluetoothLocalGattService::Create(
          adapter_.get(), BluetoothUUID(kTestUUIDGenericAttribute), true,
          nullptr, delegate_.get());
  base::WeakPtr<BluetoothLocalGattService> service4 =
      BluetoothLocalGattService::Create(
          adapter_.get(), BluetoothUUID(kTestUUIDGenericAttribute), true,
          nullptr, delegate_.get());
  service2->Register(GetCallback(Call::EXPECTED),
                     GetGattErrorCallback(Call::NOT_EXPECTED));
  service3->Register(GetCallback(Call::EXPECTED),
                     GetGattErrorCallback(Call::NOT_EXPECTED));

  service2->Unregister(GetCallback(Call::EXPECTED),
                       GetGattErrorCallback(Call::NOT_EXPECTED));

  service4->Register(GetCallback(Call::EXPECTED),
                     GetGattErrorCallback(Call::NOT_EXPECTED));

  service3->Register(GetCallback(Call::NOT_EXPECTED),
                     GetGattErrorCallback(Call::EXPECTED));
  service3->Unregister(GetCallback(Call::EXPECTED),
                       GetGattErrorCallback(Call::NOT_EXPECTED));

  service4->Unregister(GetCallback(Call::EXPECTED),
                       GetGattErrorCallback(Call::NOT_EXPECTED));
  // TODO(rkc): Once we add a IsRegistered method to service, use it to add
  // more verification to this test.
}
#endif  // defined(OS_CHROMEOS) || defined(OS_LINUX)

}  // namespace device
