// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_remote_gatt_service_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "device/bluetooth/bluetooth_adapter_android.h"
#include "device/bluetooth/bluetooth_device_android.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic_android.h"
#include "jni/ChromeBluetoothRemoteGattService_jni.h"

using base::android::AttachCurrentThread;

namespace device {

// static
BluetoothRemoteGattServiceAndroid* BluetoothRemoteGattServiceAndroid::Create(
    BluetoothAdapterAndroid* adapter,
    BluetoothDeviceAndroid* device,
    jobject bluetooth_remote_gatt_service_wrapper,
    const std::string& instanceId) {
  BluetoothRemoteGattServiceAndroid* service =
      new BluetoothRemoteGattServiceAndroid(adapter, device, instanceId);

  JNIEnv* env = base::android::AttachCurrentThread();
  service->j_service_.Reset(Java_ChromeBluetoothRemoteGattService_create(
      env, reinterpret_cast<intptr_t>(service),
      bluetooth_remote_gatt_service_wrapper,
      base::android::ConvertUTF8ToJavaString(env, instanceId).obj()));

  return service;
}

// static
bool BluetoothRemoteGattServiceAndroid::RegisterJNI(JNIEnv* env) {
  return RegisterNativesImpl(
      env);  // Generated in ChromeBluetoothRemoteGattService_jni.h
}

base::android::ScopedJavaLocalRef<jobject>
BluetoothRemoteGattServiceAndroid::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(j_service_);
}

std::string BluetoothRemoteGattServiceAndroid::GetIdentifier() const {
  return instanceId_;
}

device::BluetoothUUID BluetoothRemoteGattServiceAndroid::GetUUID() const {
  return device::BluetoothUUID(
      ConvertJavaStringToUTF8(Java_ChromeBluetoothRemoteGattService_getUUID(
          AttachCurrentThread(), j_service_.obj())));
}

bool BluetoothRemoteGattServiceAndroid::IsLocal() const {
  return false;
}

bool BluetoothRemoteGattServiceAndroid::IsPrimary() const {
  NOTIMPLEMENTED();
  return true;
}

device::BluetoothDevice* BluetoothRemoteGattServiceAndroid::GetDevice() const {
  return device_;
}

std::vector<device::BluetoothGattCharacteristic*>
BluetoothRemoteGattServiceAndroid::GetCharacteristics() const {
  EnsureCharacteristicsCreated();
  std::vector<device::BluetoothGattCharacteristic*> characteristics;
  for (const auto& map_iter : characteristics_)
    characteristics.push_back(map_iter.second);
  return characteristics;
}

std::vector<device::BluetoothGattService*>
BluetoothRemoteGattServiceAndroid::GetIncludedServices() const {
  NOTIMPLEMENTED();
  return std::vector<device::BluetoothGattService*>();
}

device::BluetoothGattCharacteristic*
BluetoothRemoteGattServiceAndroid::GetCharacteristic(
    const std::string& identifier) const {
  EnsureCharacteristicsCreated();
  const auto& iter = characteristics_.find(identifier);
  if (iter == characteristics_.end())
    return nullptr;
  return iter->second;
}

bool BluetoothRemoteGattServiceAndroid::AddCharacteristic(
    device::BluetoothGattCharacteristic* characteristic) {
  return false;
}

bool BluetoothRemoteGattServiceAndroid::AddIncludedService(
    device::BluetoothGattService* service) {
  return false;
}

void BluetoothRemoteGattServiceAndroid::Register(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  error_callback.Run();
}

void BluetoothRemoteGattServiceAndroid::Unregister(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  error_callback.Run();
}

void BluetoothRemoteGattServiceAndroid::CreateGattRemoteCharacteristic(
    JNIEnv* env,
    jobject caller,
    const jstring& instanceId,
    jobject /* BluetoothGattCharacteristicWrapper */
    bluetooth_gatt_characteristic_wrapper) {
  std::string instanceIdString =
      base::android::ConvertJavaStringToUTF8(env, instanceId);

  DCHECK(!characteristics_.contains(instanceIdString));

  characteristics_.set(
      instanceIdString,
      BluetoothRemoteGattCharacteristicAndroid::Create(instanceIdString));
}

BluetoothRemoteGattServiceAndroid::BluetoothRemoteGattServiceAndroid(
    BluetoothAdapterAndroid* adapter,
    BluetoothDeviceAndroid* device,
    const std::string& instanceId)
    : adapter_(adapter), device_(device), instanceId_(instanceId) {}

BluetoothRemoteGattServiceAndroid::~BluetoothRemoteGattServiceAndroid() {
  Java_ChromeBluetoothRemoteGattService_onBluetoothRemoteGattServiceAndroidDestruction(
      AttachCurrentThread(), j_service_.obj());
}

void BluetoothRemoteGattServiceAndroid::EnsureCharacteristicsCreated() const {
  if (!characteristics_.empty())
    return;

  // Java call
  Java_ChromeBluetoothRemoteGattService_ensureCharacteristicsCreated(
      AttachCurrentThread(), j_service_.obj());
}

}  // namespace device
