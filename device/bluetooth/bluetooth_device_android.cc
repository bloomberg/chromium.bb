// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_device_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/strings/stringprintf.h"
#include "device/bluetooth/bluetooth_adapter_android.h"
#include "device/bluetooth/bluetooth_remote_gatt_service_android.h"
#include "jni/ChromeBluetoothDevice_jni.h"

using base::android::AttachCurrentThread;
using base::android::AppendJavaStringArrayToStringVector;

namespace device {

BluetoothDeviceAndroid* BluetoothDeviceAndroid::Create(
    BluetoothAdapterAndroid* adapter,
    jobject bluetooth_device_wrapper) {  // Java Type: bluetoothDeviceWrapper
  BluetoothDeviceAndroid* device = new BluetoothDeviceAndroid(adapter);

  device->j_device_.Reset(Java_ChromeBluetoothDevice_create(
      AttachCurrentThread(), reinterpret_cast<intptr_t>(device),
      bluetooth_device_wrapper));

  return device;
}

BluetoothDeviceAndroid::~BluetoothDeviceAndroid() {
  Java_ChromeBluetoothDevice_onBluetoothDeviceAndroidDestruction(
      AttachCurrentThread(), j_device_.obj());
}

bool BluetoothDeviceAndroid::UpdateAdvertisedUUIDs(jobject advertised_uuids) {
  return Java_ChromeBluetoothDevice_updateAdvertisedUUIDs(
      AttachCurrentThread(), j_device_.obj(), advertised_uuids);
}

// static
bool BluetoothDeviceAndroid::RegisterJNI(JNIEnv* env) {
  return RegisterNativesImpl(env);  // Generated in ChromeBluetoothDevice_jni.h
}

base::android::ScopedJavaLocalRef<jobject>
BluetoothDeviceAndroid::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(j_device_);
}

uint32 BluetoothDeviceAndroid::GetBluetoothClass() const {
  return Java_ChromeBluetoothDevice_getBluetoothClass(AttachCurrentThread(),
                                                      j_device_.obj());
}

std::string BluetoothDeviceAndroid::GetAddress() const {
  return ConvertJavaStringToUTF8(Java_ChromeBluetoothDevice_getAddress(
      AttachCurrentThread(), j_device_.obj()));
}

BluetoothDevice::VendorIDSource BluetoothDeviceAndroid::GetVendorIDSource()
    const {
  // Android API does not provide Vendor ID.
  return VENDOR_ID_UNKNOWN;
}

uint16 BluetoothDeviceAndroid::GetVendorID() const {
  // Android API does not provide Vendor ID.
  return 0;
}

uint16 BluetoothDeviceAndroid::GetProductID() const {
  // Android API does not provide Product ID.
  return 0;
}

uint16 BluetoothDeviceAndroid::GetDeviceID() const {
  // Android API does not provide Device ID.
  return 0;
}

bool BluetoothDeviceAndroid::IsPaired() const {
  return Java_ChromeBluetoothDevice_isPaired(AttachCurrentThread(),
                                             j_device_.obj());
}

bool BluetoothDeviceAndroid::IsConnected() const {
  return IsGattConnected();
}

bool BluetoothDeviceAndroid::IsGattConnected() const {
  return gatt_connected_;
}

bool BluetoothDeviceAndroid::IsConnectable() const {
  NOTIMPLEMENTED();
  return false;
}

bool BluetoothDeviceAndroid::IsConnecting() const {
  NOTIMPLEMENTED();
  return false;
}

BluetoothDevice::UUIDList BluetoothDeviceAndroid::GetUUIDs() const {
  JNIEnv* env = AttachCurrentThread();
  std::vector<std::string> uuid_strings;
  AppendJavaStringArrayToStringVector(
      env, Java_ChromeBluetoothDevice_getUuids(env, j_device_.obj()).obj(),
      &uuid_strings);
  BluetoothDevice::UUIDList uuids;
  uuids.reserve(uuid_strings.size());
  for (auto uuid_string : uuid_strings) {
    uuids.push_back(BluetoothUUID(uuid_string));
  }
  return uuids;
}

int16 BluetoothDeviceAndroid::GetInquiryRSSI() const {
  NOTIMPLEMENTED();
  return kUnknownPower;
}

int16 BluetoothDeviceAndroid::GetInquiryTxPower() const {
  NOTIMPLEMENTED();
  return kUnknownPower;
}

bool BluetoothDeviceAndroid::ExpectingPinCode() const {
  NOTIMPLEMENTED();
  return false;
}

bool BluetoothDeviceAndroid::ExpectingPasskey() const {
  NOTIMPLEMENTED();
  return false;
}

bool BluetoothDeviceAndroid::ExpectingConfirmation() const {
  NOTIMPLEMENTED();
  return false;
}

void BluetoothDeviceAndroid::GetConnectionInfo(
    const ConnectionInfoCallback& callback) {
  NOTIMPLEMENTED();
  callback.Run(ConnectionInfo());
}

void BluetoothDeviceAndroid::Connect(
    PairingDelegate* pairing_delegate,
    const base::Closure& callback,
    const ConnectErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceAndroid::SetPinCode(const std::string& pincode) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceAndroid::SetPasskey(uint32 passkey) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceAndroid::ConfirmPairing() {
  NOTIMPLEMENTED();
}

void BluetoothDeviceAndroid::RejectPairing() {
  NOTIMPLEMENTED();
}

void BluetoothDeviceAndroid::CancelPairing() {
  NOTIMPLEMENTED();
}

void BluetoothDeviceAndroid::Disconnect(const base::Closure& callback,
                                        const ErrorCallback& error_callback) {
  // TODO(scheib): Also update unit tests for BluetoothGattConnection.
  NOTIMPLEMENTED();
}

void BluetoothDeviceAndroid::Forget(const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceAndroid::ConnectToService(
    const BluetoothUUID& uuid,
    const ConnectToServiceCallback& callback,
    const ConnectToServiceErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceAndroid::ConnectToServiceInsecurely(
    const BluetoothUUID& uuid,
    const ConnectToServiceCallback& callback,
    const ConnectToServiceErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceAndroid::OnConnectionStateChange(JNIEnv* env,
                                                     jobject jcaller,
                                                     int32_t status,
                                                     bool connected) {
  gatt_connected_ = connected;
  if (gatt_connected_) {
    DidConnectGatt();
  } else {
    // TODO(scheib) Create new BluetoothDevice::ConnectErrorCode enums for
    // android values not yet represented. http://crbug.com/531058
    switch (status) {   // Constants are from android.bluetooth.BluetoothGatt.
      case 0x00000101:  // GATT_FAILURE
        return DidFailToConnectGatt(ERROR_FAILED);
      case 0x00000005:  // GATT_INSUFFICIENT_AUTHENTICATION
        return DidFailToConnectGatt(ERROR_AUTH_FAILED);
      case 0x00000000:  // GATT_SUCCESS
        return DidDisconnectGatt();
      default:
        VLOG(1) << "Unhandled status: " << status;
        return DidFailToConnectGatt(ERROR_UNKNOWN);
    }
  }
}

void BluetoothDeviceAndroid::CreateGattRemoteService(
    JNIEnv* env,
    jobject caller,
    const jstring& instanceId,
    jobject bluetooth_gatt_service_wrapper  // Java Type:
                                            // BluetoothGattServiceWrapper
    ) {
  std::string instanceIdString =
      base::android::ConvertJavaStringToUTF8(env, instanceId);

  if (gatt_services_.contains(instanceIdString))
    return;

  BluetoothRemoteGattServiceAndroid* service =
      BluetoothRemoteGattServiceAndroid::Create(
          GetAdapter(), this, bluetooth_gatt_service_wrapper, instanceIdString);

  gatt_services_.set(instanceIdString,
                     make_scoped_ptr<BluetoothGattService>(service));

  FOR_EACH_OBSERVER(BluetoothAdapter::Observer, GetAdapter()->GetObservers(),
                    GattServiceAdded(adapter_, this, service));
}

BluetoothDeviceAndroid::BluetoothDeviceAndroid(BluetoothAdapterAndroid* adapter)
    : BluetoothDevice(adapter) {}

std::string BluetoothDeviceAndroid::GetDeviceName() const {
  return ConvertJavaStringToUTF8(Java_ChromeBluetoothDevice_getDeviceName(
      AttachCurrentThread(), j_device_.obj()));
}

void BluetoothDeviceAndroid::CreateGattConnectionImpl() {
  Java_ChromeBluetoothDevice_createGattConnectionImpl(
      AttachCurrentThread(), j_device_.obj(),
      base::android::GetApplicationContext());
}

void BluetoothDeviceAndroid::DisconnectGatt() {
  Java_ChromeBluetoothDevice_disconnectGatt(AttachCurrentThread(),
                                            j_device_.obj());
}

}  // namespace device
