// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_remote_gatt_characteristic_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "device/bluetooth/bluetooth_gatt_notify_session_android.h"
#include "device/bluetooth/bluetooth_remote_gatt_service_android.h"
#include "jni/ChromeBluetoothRemoteGattCharacteristic_jni.h"

using base::android::AttachCurrentThread;

namespace device {

// static
scoped_ptr<BluetoothRemoteGattCharacteristicAndroid>
BluetoothRemoteGattCharacteristicAndroid::Create(
    const std::string& instanceId,
    jobject /* BluetoothGattCharacteristicWrapper */
    bluetooth_gatt_characteristic_wrapper,
    jobject /* ChromeBluetoothDevice */ chrome_bluetooth_device) {
  scoped_ptr<BluetoothRemoteGattCharacteristicAndroid> characteristic(
      new BluetoothRemoteGattCharacteristicAndroid(instanceId));

  characteristic->j_characteristic_.Reset(
      Java_ChromeBluetoothRemoteGattCharacteristic_create(
          AttachCurrentThread(),
          reinterpret_cast<intptr_t>(characteristic.get()),
          bluetooth_gatt_characteristic_wrapper, chrome_bluetooth_device));

  return characteristic;
}

BluetoothRemoteGattCharacteristicAndroid::
    ~BluetoothRemoteGattCharacteristicAndroid() {
  Java_ChromeBluetoothRemoteGattCharacteristic_onBluetoothRemoteGattCharacteristicAndroidDestruction(
      AttachCurrentThread(), j_characteristic_.obj());
}

// static
bool BluetoothRemoteGattCharacteristicAndroid::RegisterJNI(JNIEnv* env) {
  return RegisterNativesImpl(
      env);  // Generated in ChromeBluetoothRemoteGattCharacteristic_jni.h
}

base::android::ScopedJavaLocalRef<jobject>
BluetoothRemoteGattCharacteristicAndroid::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(j_characteristic_);
}

std::string BluetoothRemoteGattCharacteristicAndroid::GetIdentifier() const {
  return instance_id_;
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
  return value_;
}

BluetoothGattService* BluetoothRemoteGattCharacteristicAndroid::GetService()
    const {
  NOTIMPLEMENTED();
  return nullptr;
}

BluetoothGattCharacteristic::Properties
BluetoothRemoteGattCharacteristicAndroid::GetProperties() const {
  return Java_ChromeBluetoothRemoteGattCharacteristic_getProperties(
      AttachCurrentThread(), j_characteristic_.obj());
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
  // TODO(crbug.com/551634): Check characteristic properties and return a better
  // error code if notifications aren't permitted.
  if (Java_ChromeBluetoothRemoteGattCharacteristic_startNotifySession(
          AttachCurrentThread(), j_characteristic_.obj())) {
    scoped_ptr<device::BluetoothGattNotifySession> notify_session(
        new BluetoothGattNotifySessionAndroid(instance_id_));
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, base::Passed(&notify_session)));
  } else {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(error_callback,
                   BluetoothRemoteGattServiceAndroid::GATT_ERROR_FAILED));
  }
}

void BluetoothRemoteGattCharacteristicAndroid::ReadRemoteCharacteristic(
    const ValueCallback& callback,
    const ErrorCallback& error_callback) {
  if (read_pending_ || write_pending_) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(error_callback,
                              BluetoothGattService::GATT_ERROR_IN_PROGRESS));
    return;
  }

  if (!Java_ChromeBluetoothRemoteGattCharacteristic_readRemoteCharacteristic(
          AttachCurrentThread(), j_characteristic_.obj())) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(error_callback,
                   BluetoothRemoteGattServiceAndroid::GATT_ERROR_FAILED));
    return;
  }

  read_pending_ = true;
  read_callback_ = callback;
  read_error_callback_ = error_callback;
}

void BluetoothRemoteGattCharacteristicAndroid::WriteRemoteCharacteristic(
    const std::vector<uint8>& new_value,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  if (read_pending_ || write_pending_) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(error_callback,
                              BluetoothGattService::GATT_ERROR_IN_PROGRESS));
    return;
  }

  JNIEnv* env = AttachCurrentThread();
  if (!Java_ChromeBluetoothRemoteGattCharacteristic_writeRemoteCharacteristic(
          env, j_characteristic_.obj(),
          base::android::ToJavaByteArray(env, new_value).obj())) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(error_callback,
                   BluetoothRemoteGattServiceAndroid::GATT_ERROR_FAILED));
    return;
  }

  write_pending_ = true;
  write_callback_ = callback;
  write_error_callback_ = error_callback;
}

void BluetoothRemoteGattCharacteristicAndroid::OnRead(JNIEnv* env,
                                                      jobject jcaller,
                                                      int32_t status,
                                                      jbyteArray value) {
  read_pending_ = false;

  // Clear callbacks before calling to avoid reentrancy issues.
  ValueCallback read_callback = read_callback_;
  ErrorCallback read_error_callback = read_error_callback_;
  read_callback_.Reset();
  read_error_callback_.Reset();

  if (status == 0  // android.bluetooth.BluetoothGatt.GATT_SUCCESS
      && !read_callback.is_null()) {
    base::android::JavaByteArrayToByteVector(env, value, &value_);
    read_callback.Run(value_);
  } else if (!read_error_callback.is_null()) {
    read_error_callback.Run(
        BluetoothRemoteGattServiceAndroid::GetGattErrorCode(status));
  }
}

void BluetoothRemoteGattCharacteristicAndroid::OnWrite(JNIEnv* env,
                                                       jobject jcaller,
                                                       int32_t status) {
  write_pending_ = false;

  // Clear callbacks before calling to avoid reentrancy issues.
  base::Closure write_callback = write_callback_;
  ErrorCallback write_error_callback = write_error_callback_;
  write_callback_.Reset();
  write_error_callback_.Reset();

  if (status == 0  // android.bluetooth.BluetoothGatt.GATT_SUCCESS
      && !write_callback.is_null()) {
    write_callback.Run();
  } else if (!write_error_callback.is_null()) {
    write_error_callback.Run(
        BluetoothRemoteGattServiceAndroid::GetGattErrorCode(status));
  }
}

BluetoothRemoteGattCharacteristicAndroid::
    BluetoothRemoteGattCharacteristicAndroid(const std::string& instanceId)
    : instance_id_(instanceId) {}

}  // namespace device
