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
  // |bluetooth_remote_gatt_service_wrapper|.
  //
  // The ChromeBluetoothRemoteGattService instance will hold a Java reference
  // to |bluetooth_remote_gatt_service_wrapper|.
  //
  // TODO(scheib): Return a scoped_ptr<>, but then device will need to handle
  // this correctly. http://crbug.com/506416
  static BluetoothRemoteGattServiceAndroid* Create(
      BluetoothAdapterAndroid* adapter,
      BluetoothDeviceAndroid* device,
      jobject bluetooth_remote_gatt_service_wrapper,  // Java Type:
      // BluetoothRemoteGattServiceWrapper
      const std::string& instanceId);

  // Register C++ methods exposed to Java using JNI.
  static bool RegisterJNI(JNIEnv* env);

  // Returns the associated ChromeBluetoothRemoteGattService Java object.
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

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
  // |characteristics_| if it is not already there.
  void CreateGattRemoteCharacteristic(
      JNIEnv* env,
      jobject caller,
      const jstring& instanceId,
      jobject /* BluetoothGattCharacteristicWrapper */
      bluetooth_gatt_characteristic_wrapper);

 private:
  friend class BluetoothDeviceAndroid;

  BluetoothRemoteGattServiceAndroid(BluetoothAdapterAndroid* adapter,
                                    BluetoothDeviceAndroid* device,
                                    const std::string& instanceId);
  ~BluetoothRemoteGattServiceAndroid() override;

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
  std::string instanceId_;

  // Map of characteristics, keyed by characteristic identifier.
  base::ScopedPtrHashMap<std::string,
                         scoped_ptr<BluetoothRemoteGattCharacteristicAndroid>>
      characteristics_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothRemoteGattServiceAndroid);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_SERVICE_ANDROID_H_
