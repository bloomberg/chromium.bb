// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_SERVICE_ANDROID_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_SERVICE_ANDROID_H_

#include <map>
#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/macros.h"
#include "device/bluetooth/bluetooth_gatt_service.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace device {

class BluetoothAdapterAndroid;
class BluetoothDeviceAndroid;
class BluetoothRemoteGattCharacteristicAndroid;

// BluetoothRemoteGattServiceAndroid along with its owned Java class
// org.chromium.device.bluetooth.ChromeBluetoothRemoteGattService implement
// BluetoothGattService.
class DEVICE_BLUETOOTH_EXPORT BluetoothRemoteGattServiceAndroid
    : public device::BluetoothGattService {
 public:
  // Create a BluetoothRemoteGattServiceAndroid instance and associated Java
  // ChromeBluetoothRemoteGattService using the provided
  // |bluetooth_gatt_service_wrapper|.
  //
  // The ChromeBluetoothRemoteGattService instance will hold a Java reference
  // to |bluetooth_gatt_service_wrapper|.
  static scoped_ptr<BluetoothRemoteGattServiceAndroid> Create(
      BluetoothAdapterAndroid* adapter,
      BluetoothDeviceAndroid* device,
      jobject /* BluetoothGattServiceWrapper */ bluetooth_gatt_service_wrapper,
      const std::string& instance_id,
      jobject /* ChromeBluetoothDevice */ chrome_bluetooth_device);

  ~BluetoothRemoteGattServiceAndroid() override;

  // Register C++ methods exposed to Java using JNI.
  static bool RegisterJNI(JNIEnv* env);

  // Returns the associated ChromeBluetoothRemoteGattService Java object.
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  // Returns a BluetoothGattService::GattErrorCode from a given
  // android.bluetooth.BluetoothGatt error code.
  // |bluetooth_gatt_code| must not be 0 == GATT_SUCCESS.
  static BluetoothGattService::GattErrorCode GetGattErrorCode(
      int bluetooth_gatt_code);

  // Returns an android.bluetooth.BluetoothGatt error code for a given
  // BluetoothGattService::GattErrorCode value.
  static int GetAndroidErrorCode(BluetoothGattService::GattErrorCode);

  // device::BluetoothGattService overrides.
  std::string GetIdentifier() const override;
  device::BluetoothUUID GetUUID() const override;
  bool IsLocal() const override;
  bool IsPrimary() const override;
  device::BluetoothDevice* GetDevice() const override;
  std::vector<device::BluetoothGattCharacteristic*> GetCharacteristics()
      const override;
  std::vector<device::BluetoothGattService*> GetIncludedServices()
      const override;
  device::BluetoothGattCharacteristic* GetCharacteristic(
      const std::string& identifier) const override;
  bool AddCharacteristic(
      device::BluetoothGattCharacteristic* characteristic) override;
  bool AddIncludedService(device::BluetoothGattService* service) override;
  void Register(const base::Closure& callback,
                const ErrorCallback& error_callback) override;
  void Unregister(const base::Closure& callback,
                  const ErrorCallback& error_callback) override;

  // Creates a Bluetooth GATT characteristic object and adds it to
  // |characteristics_|, DCHECKing that it has not already been created.
  void CreateGattRemoteCharacteristic(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller,
      const base::android::JavaParamRef<jstring>& instance_id,
      const base::android::JavaParamRef<
          jobject>& /* BluetoothGattCharacteristicWrapper */
      bluetooth_gatt_characteristic_wrapper,
      const base::android::JavaParamRef<
          jobject>& /* ChromeBluetoothDevice */ chrome_bluetooth_device);

 private:
  BluetoothRemoteGattServiceAndroid(BluetoothAdapterAndroid* adapter,
                                    BluetoothDeviceAndroid* device,
                                    const std::string& instance_id);

  // Populates |characteristics_| from Java objects if necessary.
  void EnsureCharacteristicsCreated() const;

  // Java object org.chromium.device.bluetooth.ChromeBluetoothRemoteGattService.
  base::android::ScopedJavaGlobalRef<jobject> j_service_;

  // The adapter associated with this service. It's ok to store a raw pointer
  // here since |adapter_| indirectly owns this instance.
  BluetoothAdapterAndroid* adapter_;

  // The device this GATT service belongs to. It's ok to store a raw pointer
  // here since |device_| owns this instance.
  BluetoothDeviceAndroid* device_;

  // Adapter unique instance ID.
  std::string instance_id_;

  // Map of characteristics, keyed by characteristic identifier.
  base::ScopedPtrHashMap<std::string,
                         scoped_ptr<BluetoothRemoteGattCharacteristicAndroid>>
      characteristics_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothRemoteGattServiceAndroid);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_SERVICE_ANDROID_H_
