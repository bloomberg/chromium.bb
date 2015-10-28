// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_remote_gatt_characteristic_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "jni/ChromeBluetoothRemoteGattCharacteristic_jni.h"

using base::android::AttachCurrentThread;

namespace device {

// static
scoped_ptr<BluetoothRemoteGattCharacteristicAndroid>
BluetoothRemoteGattCharacteristicAndroid::Create(
    const std::string& instanceId,
    jobject /* BluetoothGattCharacteristicWrapper */
    bluetooth_gatt_characteristic_wrapper) {
  scoped_ptr<BluetoothRemoteGattCharacteristicAndroid> characteristic(
      new BluetoothRemoteGattCharacteristicAndroid(instanceId));

  characteristic->j_characteristic_.Reset(
      Java_ChromeBluetoothRemoteGattCharacteristic_create(
          AttachCurrentThread(), bluetooth_gatt_characteristic_wrapper));

  return characteristic;
}

BluetoothRemoteGattCharacteristicAndroid::
    ~BluetoothRemoteGattCharacteristicAndroid() {}

// static
bool BluetoothRemoteGattCharacteristicAndroid::RegisterJNI(JNIEnv* env) {
  return RegisterNativesImpl(
      env);  // Generated in ChromeBluetoothRemoteGattCharacteristic_jni.h
}

std::string BluetoothRemoteGattCharacteristicAndroid::GetIdentifier() const {
  return instanceId_;
}

BluetoothUUID BluetoothRemoteGattCharacteristicAndroid::GetUUID() const {
  return device::BluetoothUUID(ConvertJavaStringToUTF8(
      Java_ChromeBluetoothRemoteGattCharacteristic_getUUID(
          AttachCurrentThread(), j_characteristic_.obj())));
}

bool BluetoothRemoteGattCharacteristicAndroid::IsLocal() const {
  return false;
}

const std::vector<uint8>& BluetoothRemoteGattCharacteristicAndroid::GetValue()
    const {
  NOTIMPLEMENTED();
  return value_;
}

BluetoothGattService* BluetoothRemoteGattCharacteristicAndroid::GetService()
    const {
  NOTIMPLEMENTED();
  return nullptr;
}

BluetoothGattCharacteristic::Properties
BluetoothRemoteGattCharacteristicAndroid::GetProperties() const {
  NOTIMPLEMENTED();
  return 0;
}

BluetoothGattCharacteristic::Permissions
BluetoothRemoteGattCharacteristicAndroid::GetPermissions() const {
  NOTIMPLEMENTED();
  return 0;
}

bool BluetoothRemoteGattCharacteristicAndroid::IsNotifying() const {
  NOTIMPLEMENTED();
  return false;
}

std::vector<BluetoothGattDescriptor*>
BluetoothRemoteGattCharacteristicAndroid::GetDescriptors() const {
  NOTIMPLEMENTED();
  return std::vector<BluetoothGattDescriptor*>();
}

BluetoothGattDescriptor*
BluetoothRemoteGattCharacteristicAndroid::GetDescriptor(
    const std::string& identifier) const {
  NOTIMPLEMENTED();
  return nullptr;
}

bool BluetoothRemoteGattCharacteristicAndroid::AddDescriptor(
    BluetoothGattDescriptor* descriptor) {
  NOTIMPLEMENTED();
  return false;
}

bool BluetoothRemoteGattCharacteristicAndroid::UpdateValue(
    const std::vector<uint8>& value) {
  NOTIMPLEMENTED();
  return false;
}

void BluetoothRemoteGattCharacteristicAndroid::StartNotifySession(
    const NotifySessionCallback& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothRemoteGattCharacteristicAndroid::ReadRemoteCharacteristic(
    const ValueCallback& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothRemoteGattCharacteristicAndroid::WriteRemoteCharacteristic(
    const std::vector<uint8>& new_value,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

BluetoothRemoteGattCharacteristicAndroid::
    BluetoothRemoteGattCharacteristicAndroid(const std::string& instanceId)
    : instanceId_(instanceId) {}

}  // namespace device
