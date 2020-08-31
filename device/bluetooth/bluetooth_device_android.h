// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_ANDROID_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_ANDROID_H_

#include <stdint.h>

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "device/bluetooth/bluetooth_adapter_android.h"
#include "device/bluetooth/bluetooth_device.h"

namespace device {

class BluetoothUUID;

// BluetoothDeviceAndroid along with its owned Java class
// org.chromium.device.bluetooth.ChromeBluetoothDevice implement
// BluetoothDevice.
class DEVICE_BLUETOOTH_EXPORT BluetoothDeviceAndroid final
    : public BluetoothDevice {
 public:
  // Create a BluetoothDeviceAndroid instance and associated Java
  // ChromeBluetoothDevice using the provided |java_bluetooth_device_wrapper|.
  //
  // The ChromeBluetoothDevice instance will hold a Java reference
  // to |bluetooth_device_wrapper|.
  static std::unique_ptr<BluetoothDeviceAndroid> Create(
      BluetoothAdapterAndroid* adapter,
      const base::android::JavaRef<jobject>&
          bluetooth_device_wrapper);  // Java Type: bluetoothDeviceWrapper

  ~BluetoothDeviceAndroid() override;

  // Returns the associated ChromeBluetoothDevice Java object.
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  // Get owning BluetoothAdapter cast to BluetoothAdapterAndroid.
  BluetoothAdapterAndroid* GetAndroidAdapter() {
    return static_cast<BluetoothAdapterAndroid*>(adapter_);
  }

  // BluetoothDevice:
  uint32_t GetBluetoothClass() const override;
  std::string GetAddress() const override;
  VendorIDSource GetVendorIDSource() const override;
  uint16_t GetVendorID() const override;
  uint16_t GetProductID() const override;
  uint16_t GetDeviceID() const override;
  uint16_t GetAppearance() const override;
  base::Optional<std::string> GetName() const override;
  bool IsPaired() const override;
  bool IsConnected() const override;
  bool IsGattConnected() const override;
  bool IsConnectable() const override;
  bool IsConnecting() const override;
  bool ExpectingPinCode() const override;
  bool ExpectingPasskey() const override;
  bool ExpectingConfirmation() const override;
  void GetConnectionInfo(const ConnectionInfoCallback& callback) override;
  void SetConnectionLatency(ConnectionLatency connection_latency,
                            const base::Closure& callback,
                            const ErrorCallback& error_callback) override;
  void Connect(device::BluetoothDevice::PairingDelegate* pairing_delegate,
               base::OnceClosure callback,
               ConnectErrorCallback error_callback) override;
  void SetPinCode(const std::string& pincode) override;
  void SetPasskey(uint32_t passkey) override;
  void ConfirmPairing() override;
  void RejectPairing() override;
  void CancelPairing() override;
  void Disconnect(const base::Closure& callback,
                  const ErrorCallback& error_callback) override;
  void Forget(const base::Closure& callback,
              const ErrorCallback& error_callback) override;
  void ConnectToService(
      const device::BluetoothUUID& uuid,
      const ConnectToServiceCallback& callback,
      const ConnectToServiceErrorCallback& error_callback) override;
  void ConnectToServiceInsecurely(
      const device::BluetoothUUID& uuid,
      const ConnectToServiceCallback& callback,
      const ConnectToServiceErrorCallback& error_callback) override;

  // Callback indicating when GATT client has connected/disconnected.
  // See android.bluetooth.BluetoothGattCallback.onConnectionStateChange.
  void OnConnectionStateChange(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      int32_t status,
      bool connected);

  // Callback indicating when all services of the device have been
  // discovered.
  void OnGattServicesDiscovered(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller);

  // Creates Bluetooth GATT service objects and adds them to
  // BluetoothDevice::gatt_services_ if they are not already there.
  void CreateGattRemoteService(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller,
      const base::android::JavaParamRef<jstring>& instance_id,
      const base::android::JavaParamRef<jobject>&
          bluetooth_gatt_service_wrapper);  // BluetoothGattServiceWrapper

 protected:
  BluetoothDeviceAndroid(BluetoothAdapterAndroid* adapter);

  // BluetoothDevice:
  void CreateGattConnectionImpl(
      base::Optional<device::BluetoothUUID> service_uuid) override;
  void DisconnectGatt() override;

  // Java object org.chromium.device.bluetooth.ChromeBluetoothDevice.
  base::android::ScopedJavaGlobalRef<jobject> j_device_;

  bool gatt_connected_ = false;

  DISALLOW_COPY_AND_ASSIGN(BluetoothDeviceAndroid);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_ANDROID_H_
