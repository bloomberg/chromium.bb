// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_DESCRIPTOR_ANDROID_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_DESCRIPTOR_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/macros.h"
#include "device/bluetooth/bluetooth_gatt_descriptor.h"

namespace device {

// BluetoothRemoteGattDescriptorAndroid along with its owned Java class
// org.chromium.device.bluetooth.ChromeBluetoothRemoteGattDescriptor
// implement BluetootGattDescriptor.
class DEVICE_BLUETOOTH_EXPORT BluetoothRemoteGattDescriptorAndroid
    : public BluetoothGattDescriptor {
 public:
  // Create a BluetoothRemoteGattDescriptorAndroid instance and associated
  // Java ChromeBluetoothRemoteGattDescriptor using the provided
  // |bluetooth_gatt_descriptor_wrapper|.
  //
  // The ChromeBluetoothRemoteGattDescriptor instance will hold a Java
  // reference to |bluetooth_gatt_descriptor_wrapper|.
  static scoped_ptr<BluetoothRemoteGattDescriptorAndroid> Create(
      const std::string& instanceId,
      jobject /* BluetoothGattDescriptorWrapper */
      bluetooth_gatt_descriptor_wrapper,
      jobject /* chromeBluetoothDevice */ chrome_bluetooth_device);

  ~BluetoothRemoteGattDescriptorAndroid() override;

  // Register C++ methods exposed to Java using JNI.
  static bool RegisterJNI(JNIEnv* env);

  // Returns the associated ChromeBluetoothRemoteGattDescriptor Java object.
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  // BluetoothGattDescriptor interface:
  std::string GetIdentifier() const override;
  BluetoothUUID GetUUID() const override;
  bool IsLocal() const override;
  const std::vector<uint8_t>& GetValue() const override;
  BluetoothGattCharacteristic* GetCharacteristic() const override;
  BluetoothGattCharacteristic::Permissions GetPermissions() const override;
  void ReadRemoteDescriptor(const ValueCallback& callback,
                            const ErrorCallback& error_callback) override;
  void WriteRemoteDescriptor(const std::vector<uint8_t>& new_value,
                             const base::Closure& callback,
                             const ErrorCallback& error_callback) override;

 private:
  BluetoothRemoteGattDescriptorAndroid(const std::string& instanceId);

  // Java object
  // org.chromium.device.bluetooth.ChromeBluetoothRemoteGattDescriptor.
  base::android::ScopedJavaGlobalRef<jobject> j_descriptor_;

  // Adapter unique instance ID.
  std::string instance_id_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothRemoteGattDescriptorAndroid);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_DESCRIPTOR_ANDROID_H_
