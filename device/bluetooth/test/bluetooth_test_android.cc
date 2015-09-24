// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/bluetooth_test_android.h"

#include "base/logging.h"
#include "device/bluetooth/android/wrappers.h"
#include "device/bluetooth/bluetooth_adapter_android.h"
#include "device/bluetooth/bluetooth_device_android.h"
#include "device/bluetooth/test/test_bluetooth_adapter_observer.h"
#include "jni/Fakes_jni.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

namespace device {

BluetoothTestAndroid::BluetoothTestAndroid() {
}

BluetoothTestAndroid::~BluetoothTestAndroid() {
}

void BluetoothTestAndroid::SetUp() {
  // Register in SetUp so that ASSERT can be used.
  ASSERT_TRUE(RegisterNativesImpl(AttachCurrentThread()));
}

bool BluetoothTestAndroid::PlatformSupportsLowEnergy() {
  return true;
}

void BluetoothTestAndroid::InitWithDefaultAdapter() {
  adapter_ =
      BluetoothAdapterAndroid::Create(
          BluetoothAdapterWrapper_CreateWithDefaultAdapter().obj()).get();
}

void BluetoothTestAndroid::InitWithoutDefaultAdapter() {
  adapter_ = BluetoothAdapterAndroid::Create(NULL).get();
}

void BluetoothTestAndroid::InitWithFakeAdapter() {
  j_fake_bluetooth_adapter_.Reset(Java_FakeBluetoothAdapter_create(
      AttachCurrentThread(), reinterpret_cast<intptr_t>(this)));

  adapter_ =
      BluetoothAdapterAndroid::Create(j_fake_bluetooth_adapter_.obj()).get();
}

BluetoothDevice* BluetoothTestAndroid::DiscoverLowEnergyDevice(
    int device_ordinal) {
  TestBluetoothAdapterObserver observer(adapter_);
  Java_FakeBluetoothAdapter_discoverLowEnergyDevice(
      AttachCurrentThread(), j_fake_bluetooth_adapter_.obj(), device_ordinal);
  return observer.last_device();
}

void BluetoothTestAndroid::CompleteGattConnection(BluetoothDevice* device) {
  BluetoothDeviceAndroid* device_android =
      static_cast<BluetoothDeviceAndroid*>(device);

  Java_FakeBluetoothDevice_connectionStateChange(
      AttachCurrentThread(), device_android->GetJavaObject().obj(),
      0,      // android.bluetooth.BluetoothGatt.GATT_SUCCESS
      true);  // connected
}

void BluetoothTestAndroid::FailGattConnection(
    BluetoothDevice* device,
    BluetoothDevice::ConnectErrorCode error) {
  int android_error_value = 0;
  switch (error) {  // Constants are from android.bluetooth.BluetoothGatt.
    case BluetoothDevice::ERROR_FAILED:
      android_error_value = 0x00000101;  // GATT_FAILURE
      break;
    case BluetoothDevice::ERROR_AUTH_FAILED:
      android_error_value = 0x00000005;  // GATT_INSUFFICIENT_AUTHENTICATION
      break;
    case BluetoothDevice::ERROR_UNKNOWN:
    case BluetoothDevice::ERROR_INPROGRESS:
    case BluetoothDevice::ERROR_AUTH_CANCELED:
    case BluetoothDevice::ERROR_AUTH_REJECTED:
    case BluetoothDevice::ERROR_AUTH_TIMEOUT:
    case BluetoothDevice::ERROR_UNSUPPORTED_DEVICE:
      NOTREACHED() << "No translation for error code: " << error;
  }

  BluetoothDeviceAndroid* device_android =
      static_cast<BluetoothDeviceAndroid*>(device);

  Java_FakeBluetoothDevice_connectionStateChange(
      AttachCurrentThread(), device_android->GetJavaObject().obj(),
      android_error_value,
      false);  // connected
}

void BluetoothTestAndroid::CompleteGattDisconnection(BluetoothDevice* device) {
  BluetoothDeviceAndroid* device_android =
      static_cast<BluetoothDeviceAndroid*>(device);

  Java_FakeBluetoothDevice_connectionStateChange(
      AttachCurrentThread(), device_android->GetJavaObject().obj(),
      0,       // android.bluetooth.BluetoothGatt.GATT_SUCCESS
      false);  // disconnected
}

// Records that Java FakeBluetoothDevice connectGatt was called.
void BluetoothTestAndroid::OnBluetoothDeviceConnectGattCalled(JNIEnv* env,
                                                              jobject caller) {
  gatt_connection_attempt_count_++;
}

// Records that Java FakeBluetoothGatt disconnect was called.
void BluetoothTestAndroid::OnFakeBluetoothGattDisconnect(JNIEnv* env,
                                                         jobject caller) {
  gatt_disconnection_attempt_count_++;
}

}  // namespace device
