// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_gatt_characteristic.h"

#include "base/run_loop.h"
#include "device/bluetooth/bluetooth_gatt_service.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "device/bluetooth/test/bluetooth_test_android.h"
#elif defined(OS_MACOSX)
#include "device/bluetooth/test/bluetooth_test_mac.h"
#endif

namespace device {

#if defined(OS_ANDROID) || defined(OS_MACOSX)
class BluetoothGattCharacteristicTest : public BluetoothTest {
 public:
  // Create adapter_, device_, service, and provides.
  void FakeCharacteristicBoilerplate() {
    InitWithFakeAdapter();
    StartLowEnergyDiscoverySession();
    device_ = DiscoverLowEnergyDevice(3);
    device_->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                                  GetConnectErrorCallback(Call::NOT_EXPECTED));
    SimulateGattConnection(device_);
    std::vector<std::string> services;
    std::string uuid("00000000-0000-1000-8000-00805f9b34fb");
    services.push_back(uuid);
    SimulateGattServicesDiscovered(device_, services);
    ASSERT_EQ(1u, device_->GetGattServices().size());
    service_ = device_->GetGattServices()[0];
    SimulateGattCharacteristic(service_, uuid, /* properties */ 0);
    SimulateGattCharacteristic(service_, uuid, /* properties */ 0);
    characteristic1_ = service_->GetCharacteristics()[0];
    characteristic2_ = service_->GetCharacteristics()[1];
    ASSERT_EQ(2u, service_->GetCharacteristics().size());
    ResetEventCounts();
  }

  BluetoothDevice* device_ = nullptr;
  BluetoothGattService* service_ = nullptr;
  BluetoothGattCharacteristic* characteristic1_ = nullptr;
  BluetoothGattCharacteristic* characteristic2_ = nullptr;
};
#endif

#if defined(OS_ANDROID)
TEST_F(BluetoothGattCharacteristicTest, GetIdentifier) {
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  // 2 devices to verify unique IDs across them.
  BluetoothDevice* device1 = DiscoverLowEnergyDevice(3);
  BluetoothDevice* device2 = DiscoverLowEnergyDevice(4);
  device1->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                                GetConnectErrorCallback(Call::NOT_EXPECTED));
  device2->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                                GetConnectErrorCallback(Call::NOT_EXPECTED));
  SimulateGattConnection(device1);
  SimulateGattConnection(device2);

  // 3 services (all with same UUID).
  //   1 on the first device (to test characteristic instances across devices).
  //   2 on the second device (to test same device, multiple service instances).
  std::vector<std::string> services;
  std::string uuid = "00000000-0000-1000-8000-00805f9b34fb";
  services.push_back(uuid);
  SimulateGattServicesDiscovered(device1, services);
  services.push_back(uuid);
  SimulateGattServicesDiscovered(device2, services);
  BluetoothGattService* service1 = device1->GetGattServices()[0];
  BluetoothGattService* service2 = device2->GetGattServices()[0];
  BluetoothGattService* service3 = device2->GetGattServices()[1];
  // 6 characteristics (same UUID), 2 on each service.
  SimulateGattCharacteristic(service1, uuid, /* properties */ 0);
  SimulateGattCharacteristic(service1, uuid, /* properties */ 0);
  SimulateGattCharacteristic(service2, uuid, /* properties */ 0);
  SimulateGattCharacteristic(service2, uuid, /* properties */ 0);
  SimulateGattCharacteristic(service3, uuid, /* properties */ 0);
  SimulateGattCharacteristic(service3, uuid, /* properties */ 0);
  BluetoothGattCharacteristic* char1 = service1->GetCharacteristics()[0];
  BluetoothGattCharacteristic* char2 = service1->GetCharacteristics()[1];
  BluetoothGattCharacteristic* char3 = service2->GetCharacteristics()[0];
  BluetoothGattCharacteristic* char4 = service2->GetCharacteristics()[1];
  BluetoothGattCharacteristic* char5 = service3->GetCharacteristics()[0];
  BluetoothGattCharacteristic* char6 = service3->GetCharacteristics()[1];

  // All IDs are unique, even though they have the same UUID.
  EXPECT_NE(char1->GetIdentifier(), char2->GetIdentifier());
  EXPECT_NE(char1->GetIdentifier(), char3->GetIdentifier());
  EXPECT_NE(char1->GetIdentifier(), char4->GetIdentifier());
  EXPECT_NE(char1->GetIdentifier(), char5->GetIdentifier());
  EXPECT_NE(char1->GetIdentifier(), char6->GetIdentifier());

  EXPECT_NE(char2->GetIdentifier(), char3->GetIdentifier());
  EXPECT_NE(char2->GetIdentifier(), char4->GetIdentifier());
  EXPECT_NE(char2->GetIdentifier(), char5->GetIdentifier());
  EXPECT_NE(char2->GetIdentifier(), char6->GetIdentifier());

  EXPECT_NE(char3->GetIdentifier(), char4->GetIdentifier());
  EXPECT_NE(char3->GetIdentifier(), char5->GetIdentifier());
  EXPECT_NE(char3->GetIdentifier(), char6->GetIdentifier());

  EXPECT_NE(char4->GetIdentifier(), char5->GetIdentifier());
  EXPECT_NE(char4->GetIdentifier(), char6->GetIdentifier());

  EXPECT_NE(char5->GetIdentifier(), char6->GetIdentifier());
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
TEST_F(BluetoothGattCharacteristicTest, GetUUID) {
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = DiscoverLowEnergyDevice(3);
  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));
  SimulateGattConnection(device);
  std::vector<std::string> services;
  services.push_back("00000000-0000-1000-8000-00805f9b34fb");
  SimulateGattServicesDiscovered(device, services);
  BluetoothGattService* service = device->GetGattServices()[0];

  // Create 3 characteristics. Two of them are duplicates.
  std::string uuid_str1("11111111-0000-1000-8000-00805f9b34fb");
  std::string uuid_str2("22222222-0000-1000-8000-00805f9b34fb");
  BluetoothUUID uuid1(uuid_str1);
  BluetoothUUID uuid2(uuid_str2);
  SimulateGattCharacteristic(service, uuid_str1, /* properties */ 0);
  SimulateGattCharacteristic(service, uuid_str2, /* properties */ 0);
  SimulateGattCharacteristic(service, uuid_str2, /* properties */ 0);
  BluetoothGattCharacteristic* char1 = service->GetCharacteristics()[0];
  BluetoothGattCharacteristic* char2 = service->GetCharacteristics()[1];
  BluetoothGattCharacteristic* char3 = service->GetCharacteristics()[2];

  // Swap as needed to have char1 point to the the characteristic with uuid1.
  if (char2->GetUUID() == uuid1) {
    std::swap(char1, char2);
  } else if (char3->GetUUID() == uuid1) {
    std::swap(char1, char3);
  }

  EXPECT_EQ(uuid1, char1->GetUUID());
  EXPECT_EQ(uuid2, char2->GetUUID());
  EXPECT_EQ(uuid2, char3->GetUUID());
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
TEST_F(BluetoothGattCharacteristicTest, GetProperties) {
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = DiscoverLowEnergyDevice(3);
  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));
  SimulateGattConnection(device);
  std::vector<std::string> services;
  std::string uuid("00000000-0000-1000-8000-00805f9b34fb");
  services.push_back(uuid);
  SimulateGattServicesDiscovered(device, services);
  BluetoothGattService* service = device->GetGattServices()[0];

  // Create two characteristics with different properties:
  SimulateGattCharacteristic(service, uuid, /* properties */ 0);
  SimulateGattCharacteristic(service, uuid, /* properties */ 7);

  // Read the properties. Because ordering is unknown swap as necessary.
  int properties1 = service->GetCharacteristics()[0]->GetProperties();
  int properties2 = service->GetCharacteristics()[1]->GetProperties();
  if (properties2 == 0)
    std::swap(properties1, properties2);
  EXPECT_EQ(0, properties1);
  EXPECT_EQ(7, properties2);
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
// Tests ReadRemoteCharacteristic and GetValue with empty value buffer.
TEST_F(BluetoothGattCharacteristicTest, ReadRemoteCharacteristic_Empty) {
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate());

  characteristic1_->ReadRemoteCharacteristic(
      GetReadValueCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(1, gatt_read_characteristic_attempts_);
  std::vector<uint8_t> empty_vector;
  SimulateGattCharacteristicRead(characteristic1_, empty_vector);

  // Duplicate read reported from OS shouldn't cause a problem:
  SimulateGattCharacteristicRead(characteristic1_, empty_vector);

  EXPECT_EQ(empty_vector, last_read_value_);
  EXPECT_EQ(empty_vector, characteristic1_->GetValue());
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
// Tests WriteRemoteCharacteristic with empty value buffer.
TEST_F(BluetoothGattCharacteristicTest, WriteRemoteCharacteristic_Empty) {
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate());

  std::vector<uint8_t> empty_vector;
  characteristic1_->WriteRemoteCharacteristic(
      empty_vector, GetCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(1, gatt_write_characteristic_attempts_);
  SimulateGattCharacteristicWrite(characteristic1_);

  EXPECT_EQ(empty_vector, last_write_value_);
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
// Tests ReadRemoteCharacteristic completing after Chrome objects are deleted.
TEST_F(BluetoothGattCharacteristicTest, ReadRemoteCharacteristic_AfterDeleted) {
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate());

  characteristic1_->ReadRemoteCharacteristic(
      GetReadValueCallback(Call::NOT_EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));

  RememberCharacteristicForSubsequentAction(characteristic1_);
  DeleteDevice(device_);

  std::vector<uint8_t> empty_vector;
  SimulateGattCharacteristicRead(/* use remembered characteristic */ nullptr,
                                 empty_vector);
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
// Tests WriteRemoteCharacteristic completing after Chrome objects are deleted.
TEST_F(BluetoothGattCharacteristicTest,
       WriteRemoteCharacteristic_AfterDeleted) {
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate());

  std::vector<uint8_t> empty_vector;
  characteristic1_->WriteRemoteCharacteristic(
      empty_vector, GetCallback(Call::NOT_EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));

  RememberCharacteristicForSubsequentAction(characteristic1_);
  DeleteDevice(device_);

  SimulateGattCharacteristicWrite(/* use remembered characteristic */ nullptr);
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
// Tests ReadRemoteCharacteristic and GetValue with non-empty value buffer.
TEST_F(BluetoothGattCharacteristicTest, ReadRemoteCharacteristic) {
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate());

  characteristic1_->ReadRemoteCharacteristic(
      GetReadValueCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(1, gatt_read_characteristic_attempts_);

  uint8_t values[] = {0, 1, 2, 3, 4, 0xf, 0xf0, 0xff};
  std::vector<uint8_t> test_vector(values, values + arraysize(values));
  SimulateGattCharacteristicRead(characteristic1_, test_vector);

  // Duplicate read reported from OS shouldn't cause a problem:
  std::vector<uint8_t> empty_vector;
  SimulateGattCharacteristicRead(characteristic1_, empty_vector);

  EXPECT_EQ(test_vector, last_read_value_);
  EXPECT_EQ(test_vector, characteristic1_->GetValue());
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
// Tests WriteRemoteCharacteristic with non-empty value buffer.
TEST_F(BluetoothGattCharacteristicTest, WriteRemoteCharacteristic) {
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate());

  uint8_t values[] = {0, 1, 2, 3, 4, 0xf, 0xf0, 0xff};
  std::vector<uint8_t> test_vector(values, values + arraysize(values));
  characteristic1_->WriteRemoteCharacteristic(
      test_vector, GetCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(1, gatt_write_characteristic_attempts_);

  SimulateGattCharacteristicWrite(characteristic1_);

  EXPECT_EQ(test_vector, last_write_value_);
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
// Tests ReadRemoteCharacteristic and GetValue multiple times.
TEST_F(BluetoothGattCharacteristicTest, ReadRemoteCharacteristic_Twice) {
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate());

  characteristic1_->ReadRemoteCharacteristic(
      GetReadValueCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(1, gatt_read_characteristic_attempts_);

  uint8_t values[] = {0, 1, 2, 3, 4, 0xf, 0xf0, 0xff};
  std::vector<uint8_t> test_vector(values, values + arraysize(values));
  SimulateGattCharacteristicRead(characteristic1_, test_vector);
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(test_vector, last_read_value_);
  EXPECT_EQ(test_vector, characteristic1_->GetValue());

  // Read again, with different value:
  ResetEventCounts();
  characteristic1_->ReadRemoteCharacteristic(
      GetReadValueCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(1, gatt_read_characteristic_attempts_);
  std::vector<uint8_t> empty_vector;
  SimulateGattCharacteristicRead(characteristic1_, empty_vector);
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(empty_vector, last_read_value_);
  EXPECT_EQ(empty_vector, characteristic1_->GetValue());
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
// Tests WriteRemoteCharacteristic multiple times.
TEST_F(BluetoothGattCharacteristicTest, WriteRemoteCharacteristic_Twice) {
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate());

  uint8_t values[] = {0, 1, 2, 3, 4, 0xf, 0xf0, 0xff};
  std::vector<uint8_t> test_vector(values, values + arraysize(values));
  characteristic1_->WriteRemoteCharacteristic(
      test_vector, GetCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(1, gatt_write_characteristic_attempts_);

  SimulateGattCharacteristicWrite(characteristic1_);
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(test_vector, last_write_value_);

  // Write again, with different value:
  ResetEventCounts();
  std::vector<uint8_t> empty_vector;
  characteristic1_->WriteRemoteCharacteristic(
      empty_vector, GetCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(1, gatt_write_characteristic_attempts_);
  SimulateGattCharacteristicWrite(characteristic1_);
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(empty_vector, last_write_value_);
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
// Tests ReadRemoteCharacteristic on two characteristics.
TEST_F(BluetoothGattCharacteristicTest,
       ReadRemoteCharacteristic_MultipleCharacteristics) {
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate());

  characteristic1_->ReadRemoteCharacteristic(
      GetReadValueCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  characteristic2_->ReadRemoteCharacteristic(
      GetReadValueCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(2, gatt_read_characteristic_attempts_);
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  std::vector<uint8_t> test_vector1;
  test_vector1.push_back(111);
  SimulateGattCharacteristicRead(characteristic1_, test_vector1);
  EXPECT_EQ(test_vector1, last_read_value_);

  std::vector<uint8_t> test_vector2;
  test_vector2.push_back(222);
  SimulateGattCharacteristicRead(characteristic2_, test_vector2);
  EXPECT_EQ(test_vector2, last_read_value_);

  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(test_vector1, characteristic1_->GetValue());
  EXPECT_EQ(test_vector2, characteristic2_->GetValue());
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
// Tests WriteRemoteCharacteristic on two characteristics.
TEST_F(BluetoothGattCharacteristicTest,
       WriteRemoteCharacteristic_MultipleCharacteristics) {
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate());

  std::vector<uint8_t> test_vector1;
  test_vector1.push_back(111);
  characteristic1_->WriteRemoteCharacteristic(
      test_vector1, GetCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(test_vector1, last_write_value_);

  std::vector<uint8_t> test_vector2;
  test_vector2.push_back(222);
  characteristic2_->WriteRemoteCharacteristic(
      test_vector2, GetCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(test_vector2, last_write_value_);

  EXPECT_EQ(2, gatt_write_characteristic_attempts_);
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  SimulateGattCharacteristicWrite(characteristic1_);
  SimulateGattCharacteristicWrite(characteristic2_);

  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
// Tests ReadRemoteCharacteristic asynchronous error.
TEST_F(BluetoothGattCharacteristicTest, ReadError) {
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate());

  characteristic1_->ReadRemoteCharacteristic(
      GetReadValueCallback(Call::NOT_EXPECTED),
      GetGattErrorCallback(Call::EXPECTED));
  SimulateGattCharacteristicReadError(
      characteristic1_, BluetoothGattService::GATT_ERROR_INVALID_LENGTH);
  SimulateGattCharacteristicReadError(characteristic1_,
                                      BluetoothGattService::GATT_ERROR_FAILED);
  EXPECT_EQ(BluetoothGattService::GATT_ERROR_INVALID_LENGTH,
            last_gatt_error_code_);
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
// Tests WriteRemoteCharacteristic asynchronous error.
TEST_F(BluetoothGattCharacteristicTest, WriteError) {
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate());

  std::vector<uint8_t> empty_vector;
  characteristic1_->WriteRemoteCharacteristic(
      empty_vector, GetCallback(Call::NOT_EXPECTED),
      GetGattErrorCallback(Call::EXPECTED));
  SimulateGattCharacteristicWriteError(
      characteristic1_, BluetoothGattService::GATT_ERROR_INVALID_LENGTH);
  SimulateGattCharacteristicWriteError(characteristic1_,
                                       BluetoothGattService::GATT_ERROR_FAILED);

  EXPECT_EQ(BluetoothGattService::GATT_ERROR_INVALID_LENGTH,
            last_gatt_error_code_);
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
// Tests ReadRemoteCharacteristic synchronous error.
TEST_F(BluetoothGattCharacteristicTest, ReadSynchronousError) {
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate());

  SimulateGattCharacteristicReadWillFailSynchronouslyOnce(characteristic1_);
  characteristic1_->ReadRemoteCharacteristic(
      GetReadValueCallback(Call::NOT_EXPECTED),
      GetGattErrorCallback(Call::EXPECTED));
  EXPECT_EQ(0, gatt_read_characteristic_attempts_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothGattService::GATT_ERROR_FAILED, last_gatt_error_code_);

  // After failing once, can succeed:
  ResetEventCounts();
  characteristic1_->ReadRemoteCharacteristic(
      GetReadValueCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(1, gatt_read_characteristic_attempts_);
  std::vector<uint8_t> empty_vector;
  SimulateGattCharacteristicRead(characteristic1_, empty_vector);
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
// Tests WriteRemoteCharacteristic synchronous error.
TEST_F(BluetoothGattCharacteristicTest, WriteSynchronousError) {
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate());

  SimulateGattCharacteristicWriteWillFailSynchronouslyOnce(characteristic1_);
  std::vector<uint8_t> empty_vector;
  characteristic1_->WriteRemoteCharacteristic(
      empty_vector, GetCallback(Call::NOT_EXPECTED),
      GetGattErrorCallback(Call::EXPECTED));
  EXPECT_EQ(0, gatt_write_characteristic_attempts_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothGattService::GATT_ERROR_FAILED, last_gatt_error_code_);

  // After failing once, can succeed:
  ResetEventCounts();
  characteristic1_->WriteRemoteCharacteristic(
      empty_vector, GetCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(1, gatt_write_characteristic_attempts_);
  SimulateGattCharacteristicWrite(characteristic1_);
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
// Tests ReadRemoteCharacteristic error with a pending read operation.
TEST_F(BluetoothGattCharacteristicTest, ReadRemoteCharacteristic_ReadPending) {
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate());

  characteristic1_->ReadRemoteCharacteristic(
      GetReadValueCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  characteristic1_->ReadRemoteCharacteristic(
      GetReadValueCallback(Call::NOT_EXPECTED),
      GetGattErrorCallback(Call::EXPECTED));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothGattService::GATT_ERROR_IN_PROGRESS,
            last_gatt_error_code_);

  // Initial read should still succeed:
  ResetEventCounts();
  std::vector<uint8_t> empty_vector;
  SimulateGattCharacteristicRead(characteristic1_, empty_vector);
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
// Tests WriteRemoteCharacteristic error with a pending write operation.
TEST_F(BluetoothGattCharacteristicTest,
       WriteRemoteCharacteristic_WritePending) {
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate());

  std::vector<uint8_t> empty_vector;
  characteristic1_->WriteRemoteCharacteristic(
      empty_vector, GetCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  characteristic1_->WriteRemoteCharacteristic(
      empty_vector, GetCallback(Call::NOT_EXPECTED),
      GetGattErrorCallback(Call::EXPECTED));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothGattService::GATT_ERROR_IN_PROGRESS,
            last_gatt_error_code_);

  // Initial write should still succeed:
  ResetEventCounts();
  SimulateGattCharacteristicWrite(characteristic1_);
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
// Tests ReadRemoteCharacteristic error with a pending write operation.
TEST_F(BluetoothGattCharacteristicTest, ReadRemoteCharacteristic_WritePending) {
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate());

  std::vector<uint8_t> empty_vector;
  characteristic1_->WriteRemoteCharacteristic(
      empty_vector, GetCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  characteristic1_->ReadRemoteCharacteristic(
      GetReadValueCallback(Call::NOT_EXPECTED),
      GetGattErrorCallback(Call::EXPECTED));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothGattService::GATT_ERROR_IN_PROGRESS,
            last_gatt_error_code_);

  // Initial write should still succeed:
  ResetEventCounts();
  SimulateGattCharacteristicWrite(characteristic1_);
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
// Tests WriteRemoteCharacteristic error with a pending Read operation.
TEST_F(BluetoothGattCharacteristicTest, WriteRemoteCharacteristic_ReadPending) {
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate());

  std::vector<uint8_t> empty_vector;
  characteristic1_->ReadRemoteCharacteristic(
      GetReadValueCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  characteristic1_->WriteRemoteCharacteristic(
      empty_vector, GetCallback(Call::NOT_EXPECTED),
      GetGattErrorCallback(Call::EXPECTED));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothGattService::GATT_ERROR_IN_PROGRESS,
            last_gatt_error_code_);

  // Initial read should still succeed:
  ResetEventCounts();
  SimulateGattCharacteristicRead(characteristic1_, empty_vector);
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
// Tests StartNotifySession success.
TEST_F(BluetoothGattCharacteristicTest, StartNotifySession) {
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate());

  characteristic1_->StartNotifySession(
      GetNotifyCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(1, gatt_notify_characteristic_attempts_);
  EXPECT_EQ(0, callback_count_);
  SimulateGattNotifySessionStarted(characteristic1_);
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  ASSERT_EQ(1u, notify_sessions_.size());
  ASSERT_TRUE(notify_sessions_[0]);
  EXPECT_EQ(characteristic1_->GetIdentifier(),
            notify_sessions_[0]->GetCharacteristicIdentifier());
  EXPECT_TRUE(notify_sessions_[0]->IsActive());
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
// Tests StartNotifySession synchronous failure.
TEST_F(BluetoothGattCharacteristicTest, StartNotifySession_SynchronousError) {
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate());

  SimulateGattCharacteristicSetNotifyWillFailSynchronouslyOnce(
      characteristic1_);
  characteristic1_->StartNotifySession(GetNotifyCallback(Call::NOT_EXPECTED),
                                       GetGattErrorCallback(Call::EXPECTED));
  EXPECT_EQ(0, error_callback_count_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, gatt_notify_characteristic_attempts_);
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  ASSERT_EQ(0u, notify_sessions_.size());
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
// Tests multiple StartNotifySession success.
TEST_F(BluetoothGattCharacteristicTest, StartNotifySession_Multiple) {
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate());

  characteristic1_->StartNotifySession(
      GetNotifyCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  characteristic1_->StartNotifySession(
      GetNotifyCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
#if defined(OS_ANDROID)
  // TODO(crbug.com/551634): Decide when implementing IsNotifying if Android
  // should trust the notification request always worked, or if we should always
  // redundantly set the value to the OS.
  EXPECT_EQ(2, gatt_notify_characteristic_attempts_);
#else
  EXPECT_EQ(1, gatt_notify_characteristic_attempts_);
#endif
  EXPECT_EQ(0, callback_count_);
  SimulateGattNotifySessionStarted(characteristic1_);
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  ASSERT_EQ(2u, notify_sessions_.size());
  ASSERT_TRUE(notify_sessions_[0]);
  ASSERT_TRUE(notify_sessions_[1]);
  EXPECT_EQ(characteristic1_->GetIdentifier(),
            notify_sessions_[0]->GetCharacteristicIdentifier());
  EXPECT_EQ(characteristic1_->GetIdentifier(),
            notify_sessions_[1]->GetCharacteristicIdentifier());
  EXPECT_TRUE(notify_sessions_[0]->IsActive());
  EXPECT_TRUE(notify_sessions_[1]->IsActive());
}
#endif  // defined(OS_ANDROID)

}  // namespace device
