// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_ANDROID_H_
#define DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_ANDROID_H_

#include <stdint.h>

#include "base/android/scoped_java_ref.h"
#include "device/bluetooth/test/bluetooth_test.h"

namespace device {

class BluetoothAdapterAndroid;

// Android implementation of BluetoothTestBase.
class BluetoothTestAndroid : public BluetoothTestBase {
 public:
  BluetoothTestAndroid();
  ~BluetoothTestAndroid() override;

  // Test overrides:
  void SetUp() override;
  void TearDown() override;

  // BluetoothTestBase overrides:
  bool PlatformSupportsLowEnergy() override;
  void InitWithDefaultAdapter() override;
  void InitWithoutDefaultAdapter() override;
  void InitWithFakeAdapter() override;
  bool DenyPermission() override;
  BluetoothDevice* DiscoverLowEnergyDevice(int device_ordinal) override;
  void SimulateGattConnection(BluetoothDevice* device) override;
  void SimulateGattConnectionError(BluetoothDevice* device,
                                   BluetoothDevice::ConnectErrorCode) override;
  void SimulateGattDisconnection(BluetoothDevice* device) override;
  void SimulateGattServicesDiscovered(
      BluetoothDevice* device,
      const std::vector<std::string>& uuids) override;
  void SimulateGattServicesDiscoveryError(BluetoothDevice* device) override;
  void SimulateGattCharacteristic(BluetoothGattService* service,
                                  const std::string& uuid,
                                  int properties) override;
  void RememberCharacteristicForSubsequentAction(
      BluetoothGattCharacteristic* characteristic) override;
  void SimulateGattNotifySessionStarted(
      BluetoothGattCharacteristic* characteristic) override;
  void SimulateGattCharacteristicSetNotifyWillFailSynchronouslyOnce(
      BluetoothGattCharacteristic* characteristic) override;
  void SimulateGattCharacteristicChanged(
      BluetoothGattCharacteristic* characteristic,
      const std::vector<uint8_t>& value) override;
  void SimulateGattCharacteristicRead(
      BluetoothGattCharacteristic* characteristic,
      const std::vector<uint8_t>& value) override;
  void SimulateGattCharacteristicReadError(
      BluetoothGattCharacteristic* characteristic,
      BluetoothGattService::GattErrorCode) override;
  void SimulateGattCharacteristicReadWillFailSynchronouslyOnce(
      BluetoothGattCharacteristic* characteristic) override;
  void SimulateGattCharacteristicWrite(
      BluetoothGattCharacteristic* characteristic) override;
  void SimulateGattCharacteristicWriteError(
      BluetoothGattCharacteristic* characteristic,
      BluetoothGattService::GattErrorCode) override;
  void SimulateGattCharacteristicWriteWillFailSynchronouslyOnce(
      BluetoothGattCharacteristic* characteristic) override;
  void SimulateGattDescriptor(BluetoothGattCharacteristic* characteristic,
                              const std::string& uuid) override;
  void SimulateGattDescriptorWriteWillFailSynchronouslyOnce(
      BluetoothGattDescriptor* descriptor) override;

  // Records that Java FakeBluetoothDevice connectGatt was called.
  void OnFakeBluetoothDeviceConnectGattCalled(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller);

  // Records that Java FakeBluetoothGatt disconnect was called.
  void OnFakeBluetoothGattDisconnect(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller);

  // Records that Java FakeBluetoothGatt close was called.
  void OnFakeBluetoothGattClose(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller);

  // Records that Java FakeBluetoothGatt discoverServices was called.
  void OnFakeBluetoothGattDiscoverServices(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller);

  // Records that Java FakeBluetoothGatt setCharacteristicNotification was
  // called.
  void OnFakeBluetoothGattSetCharacteristicNotification(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller);

  // Records that Java FakeBluetoothGatt readCharacteristic was called.
  void OnFakeBluetoothGattReadCharacteristic(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller);

  // Records that Java FakeBluetoothGatt writeCharacteristic was called.
  void OnFakeBluetoothGattWriteCharacteristic(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller,
      const base::android::JavaParamRef<jbyteArray>& value);

  // Records that Java FakeBluetoothGatt writeDescriptor was called.
  void OnFakeBluetoothGattWriteDescriptor(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller,
      const base::android::JavaParamRef<jbyteArray>& value);

  base::android::ScopedJavaGlobalRef<jobject> j_fake_bluetooth_adapter_;

  int gatt_open_connections_ = 0;
};

// Defines common test fixture name. Use TEST_F(BluetoothTest, YourTestName).
typedef BluetoothTestAndroid BluetoothTest;

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_ANDROID_H_
