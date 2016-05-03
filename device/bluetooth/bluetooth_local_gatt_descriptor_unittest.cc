// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_local_gatt_descriptor.h"
#include "device/bluetooth/test/bluetooth_gatt_server_test.h"
#include "device/bluetooth/test/bluetooth_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class BluetoothLocalGattDescriptorTest : public BluetoothGattServerTest {
 public:
  void SetUp() override {
    BluetoothGattServerTest::SetUp();

    StartGattSetup();
    characteristic_ = BluetoothLocalGattCharacteristic::Create(
        BluetoothUUID(kTestUUIDGenericAttribute),
        device::BluetoothLocalGattCharacteristic::Properties(),
        device::BluetoothLocalGattCharacteristic::Permissions(),
        service_.get());
    descriptor_ = BluetoothLocalGattDescriptor::Create(
        BluetoothUUID(kTestUUIDGenericAttribute),
        device::BluetoothLocalGattCharacteristic::Permissions(),
        characteristic_.get());
    EXPECT_LT(0u, descriptor_->GetIdentifier().size());
    CompleteGattSetup();
  }

 protected:
  base::WeakPtr<BluetoothLocalGattCharacteristic> characteristic_;
  base::WeakPtr<BluetoothLocalGattDescriptor> descriptor_;
};

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
TEST_F(BluetoothLocalGattDescriptorTest, ReadLocalDescriptorValue) {
  delegate_->value_to_write_ = 0x1337;
  SimulateLocalGattDescriptorValueReadRequest(
      service_.get(), descriptor_.get(), GetReadValueCallback(Call::EXPECTED),
      GetCallback(Call::NOT_EXPECTED));

  EXPECT_EQ(delegate_->value_to_write_, GetInteger(last_read_value_));
}
#endif  // defined(OS_CHROMEOS) || defined(OS_LINUX)

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
TEST_F(BluetoothLocalGattDescriptorTest, WriteLocalDescriptorValue) {
  const uint64_t kValueToWrite = 0x7331ul;
  SimulateLocalGattDescriptorValueWriteRequest(
      service_.get(), descriptor_.get(), GetValue(kValueToWrite),
      GetCallback(Call::EXPECTED), GetCallback(Call::NOT_EXPECTED));

  EXPECT_EQ(kValueToWrite, delegate_->last_written_value_);
}
#endif  // defined(OS_CHROMEOS) || defined(OS_LINUX)

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
TEST_F(BluetoothLocalGattDescriptorTest, ReadLocalDescriptorValueFail) {
  delegate_->value_to_write_ = 0x1337;
  delegate_->should_fail_ = true;
  SimulateLocalGattDescriptorValueReadRequest(
      service_.get(), descriptor_.get(),
      GetReadValueCallback(Call::NOT_EXPECTED), GetCallback(Call::EXPECTED));

  EXPECT_NE(delegate_->value_to_write_, GetInteger(last_read_value_));
}
#endif  // defined(OS_CHROMEOS) || defined(OS_LINUX)

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
TEST_F(BluetoothLocalGattDescriptorTest, WriteLocalDescriptorValueFail) {
  const uint64_t kValueToWrite = 0x7331ul;
  delegate_->should_fail_ = true;
  SimulateLocalGattDescriptorValueWriteRequest(
      service_.get(), descriptor_.get(), GetValue(kValueToWrite),
      GetCallback(Call::NOT_EXPECTED), GetCallback(Call::EXPECTED));

  EXPECT_NE(kValueToWrite, delegate_->last_written_value_);
}
#endif  // defined(OS_CHROMEOS) || defined(OS_LINUX)

}  // namespace device
