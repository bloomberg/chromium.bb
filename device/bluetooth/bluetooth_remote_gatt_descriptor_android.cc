// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_remote_gatt_descriptor_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "device/bluetooth/bluetooth_gatt_notify_session_android.h"
#include "device/bluetooth/bluetooth_remote_gatt_service_android.h"
#include "jni/ChromeBluetoothRemoteGattDescriptor_jni.h"

using base::android::AttachCurrentThread;

namespace device {

// static
scoped_ptr<BluetoothRemoteGattDescriptorAndroid>
BluetoothRemoteGattDescriptorAndroid::Create(
    const std::string& instance_id,
    jobject /* BluetoothGattDescriptorWrapper */
    bluetooth_gatt_descriptor_wrapper,
    jobject /* chromeBluetoothDevice */
    chrome_bluetooth_device) {
  scoped_ptr<BluetoothRemoteGattDescriptorAndroid> descriptor(
      new BluetoothRemoteGattDescriptorAndroid(instance_id));

  descriptor->j_descriptor_.Reset(
      Java_ChromeBluetoothRemoteGattDescriptor_create(
          AttachCurrentThread(),
          // TODO(scheib) Will eventually need to pass c++ pointer:
          //     reinterpret_cast<intptr_t>(descriptor.get()),
          bluetooth_gatt_descriptor_wrapper, chrome_bluetooth_device));

  return descriptor;
}

BluetoothRemoteGattDescriptorAndroid::~BluetoothRemoteGattDescriptorAndroid() {
  Java_ChromeBluetoothRemoteGattDescriptor_onBluetoothRemoteGattDescriptorAndroidDestruction(
      AttachCurrentThread(), j_descriptor_.obj());
}

// static
bool BluetoothRemoteGattDescriptorAndroid::RegisterJNI(JNIEnv* env) {
  return RegisterNativesImpl(
      env);  // Generated in ChromeBluetoothRemoteGattDescriptor_jni.h
}

base::android::ScopedJavaLocalRef<jobject>
BluetoothRemoteGattDescriptorAndroid::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(j_descriptor_);
}

std::string BluetoothRemoteGattDescriptorAndroid::GetIdentifier() const {
  return instance_id_;
}

BluetoothUUID BluetoothRemoteGattDescriptorAndroid::GetUUID() const {
  return device::BluetoothUUID(
      ConvertJavaStringToUTF8(Java_ChromeBluetoothRemoteGattDescriptor_getUUID(
          AttachCurrentThread(), j_descriptor_.obj())));
}

bool BluetoothRemoteGattDescriptorAndroid::IsLocal() const {
  return false;
}

const std::vector<uint8_t>& BluetoothRemoteGattDescriptorAndroid::GetValue()
    const {
  NOTIMPLEMENTED();
  static std::vector<uint8_t> empty_value;
  return empty_value;
}

BluetoothGattCharacteristic*
BluetoothRemoteGattDescriptorAndroid::GetCharacteristic() const {
  NOTIMPLEMENTED();
  return nullptr;
}

BluetoothGattCharacteristic::Permissions
BluetoothRemoteGattDescriptorAndroid::GetPermissions() const {
  NOTIMPLEMENTED();
  return 0;
}

void BluetoothRemoteGattDescriptorAndroid::ReadRemoteDescriptor(
    const ValueCallback& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(error_callback, BluetoothGattService::GATT_ERROR_FAILED));
}

void BluetoothRemoteGattDescriptorAndroid::WriteRemoteDescriptor(
    const std::vector<uint8_t>& new_value,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(error_callback, BluetoothGattService::GATT_ERROR_FAILED));
}

BluetoothRemoteGattDescriptorAndroid::BluetoothRemoteGattDescriptorAndroid(
    const std::string& instance_id)
    : instance_id_(instance_id) {}

}  // namespace device
