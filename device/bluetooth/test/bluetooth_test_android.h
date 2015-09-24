// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_ANDROID_H_
#define DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_ANDROID_H_

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

  // BluetoothTestBase overrides:
  bool PlatformSupportsLowEnergy() override;
  void InitWithDefaultAdapter() override;
  void InitWithoutDefaultAdapter() override;
  void InitWithFakeAdapter() override;
  BluetoothDevice* DiscoverLowEnergyDevice(int device_ordinal) override;
  void CompleteGattConnection(BluetoothDevice* device) override;
  void FailGattConnection(BluetoothDevice* device,
                          BluetoothDevice::ConnectErrorCode) override;
  void CompleteGattDisconnection(BluetoothDevice* device) override;

  // Records that Java FakeBluetoothDevice connectGatt was called.
  void OnBluetoothDeviceConnectGattCalled(JNIEnv* env, jobject caller);

  // Records that Java FakeBluetoothGatt disconnect was called.
  void OnFakeBluetoothGattDisconnect(JNIEnv* env, jobject caller);

  base::android::ScopedJavaGlobalRef<jobject> j_fake_bluetooth_adapter_;
};

// Defines common test fixture name. Use TEST_F(BluetoothTest, YourTestName).
typedef BluetoothTestAndroid BluetoothTest;

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_ANDROID_H_
