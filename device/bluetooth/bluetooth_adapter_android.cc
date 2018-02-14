// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_adapter_android.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "device/bluetooth/android/wrappers.h"
#include "device/bluetooth/bluetooth_advertisement.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_device_android.h"
#include "device/bluetooth/bluetooth_discovery_session_outcome.h"
#include "jni/ChromeBluetoothAdapter_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::AppendJavaStringArrayToStringVector;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::JavaByteArrayToByteVector;
using base::android::JavaArrayOfByteArrayToStringVector;
using base::android::JavaIntArrayToIntVector;

namespace {
// The poll interval in ms when there is no active discovery. This
// matches the max allowed advertisting interval for connectable
// devices.
enum { kPassivePollInterval = 11000 };
// The poll interval in ms when there is an active discovery.
enum { kActivePollInterval = 1000 };
// The delay in ms to wait before purging devices when a scan starts.
enum { kPurgeDelay = 500 };
}

namespace device {

// static
base::WeakPtr<BluetoothAdapter> BluetoothAdapter::CreateAdapter(
    const InitCallback& init_callback) {
  return BluetoothAdapterAndroid::Create(
      BluetoothAdapterWrapper_CreateWithDefaultAdapter());
}

// static
base::WeakPtr<BluetoothAdapterAndroid> BluetoothAdapterAndroid::Create(
    const JavaRef<jobject>&
        bluetooth_adapter_wrapper) {  // Java Type: bluetoothAdapterWrapper
  BluetoothAdapterAndroid* adapter = new BluetoothAdapterAndroid();

  adapter->j_adapter_.Reset(Java_ChromeBluetoothAdapter_create(
      AttachCurrentThread(), reinterpret_cast<intptr_t>(adapter),
      bluetooth_adapter_wrapper));

  adapter->ui_task_runner_ = base::ThreadTaskRunnerHandle::Get();

  return adapter->weak_ptr_factory_.GetWeakPtr();
}

std::string BluetoothAdapterAndroid::GetAddress() const {
  return ConvertJavaStringToUTF8(Java_ChromeBluetoothAdapter_getAddress(
      AttachCurrentThread(), j_adapter_));
}

std::string BluetoothAdapterAndroid::GetName() const {
  return ConvertJavaStringToUTF8(
      Java_ChromeBluetoothAdapter_getName(AttachCurrentThread(), j_adapter_));
}

void BluetoothAdapterAndroid::SetName(const std::string& name,
                                      const base::Closure& callback,
                                      const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

bool BluetoothAdapterAndroid::IsInitialized() const {
  return true;
}

bool BluetoothAdapterAndroid::IsPresent() const {
  return Java_ChromeBluetoothAdapter_isPresent(AttachCurrentThread(),
                                               j_adapter_);
}

bool BluetoothAdapterAndroid::IsPowered() const {
  return Java_ChromeBluetoothAdapter_isPowered(AttachCurrentThread(),
                                               j_adapter_);
}

bool BluetoothAdapterAndroid::IsDiscoverable() const {
  return Java_ChromeBluetoothAdapter_isDiscoverable(AttachCurrentThread(),
                                                    j_adapter_);
}

void BluetoothAdapterAndroid::SetDiscoverable(
    bool discoverable,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

bool BluetoothAdapterAndroid::IsDiscovering() const {
  return Java_ChromeBluetoothAdapter_isDiscovering(AttachCurrentThread(),
                                                   j_adapter_);
}

BluetoothAdapter::UUIDList BluetoothAdapterAndroid::GetUUIDs() const {
  NOTIMPLEMENTED();
  return UUIDList();
}

void BluetoothAdapterAndroid::CreateRfcommService(
    const BluetoothUUID& uuid,
    const ServiceOptions& options,
    const CreateServiceCallback& callback,
    const CreateServiceErrorCallback& error_callback) {
  NOTIMPLEMENTED();
  error_callback.Run("Not Implemented");
}

void BluetoothAdapterAndroid::CreateL2capService(
    const BluetoothUUID& uuid,
    const ServiceOptions& options,
    const CreateServiceCallback& callback,
    const CreateServiceErrorCallback& error_callback) {
  NOTIMPLEMENTED();
  error_callback.Run("Not Implemented");
}

void BluetoothAdapterAndroid::RegisterAdvertisement(
    std::unique_ptr<BluetoothAdvertisement::Data> advertisement_data,
    const CreateAdvertisementCallback& callback,
    const AdvertisementErrorCallback& error_callback) {
  error_callback.Run(BluetoothAdvertisement::ERROR_UNSUPPORTED_PLATFORM);
}

BluetoothLocalGattService* BluetoothAdapterAndroid::GetGattService(
    const std::string& identifier) const {
  return nullptr;
}

void BluetoothAdapterAndroid::OnAdapterStateChanged(
    JNIEnv* env,
    const JavaParamRef<jobject>& caller,
    const bool powered) {
  DidChangePoweredState();
  NotifyAdapterPoweredChanged(powered);
}

void BluetoothAdapterAndroid::OnScanFailed(
    JNIEnv* env,
    const JavaParamRef<jobject>& caller) {
  num_discovery_sessions_ = 0;
  MarkDiscoverySessionsAsInactive();
}

void BluetoothAdapterAndroid::CreateOrUpdateDeviceOnScan(
    JNIEnv* env,
    const JavaParamRef<jobject>& caller,
    const JavaParamRef<jstring>& address,
    const JavaParamRef<jobject>&
        bluetooth_device_wrapper,  // Java Type: bluetoothDeviceWrapper
    int32_t rssi,
    const JavaParamRef<jobjectArray>& advertised_uuids,  // Java Type: String[]
    int32_t tx_power,
    const JavaParamRef<jobjectArray>& service_data_keys,  // Java Type: String[]
    const JavaParamRef<jobjectArray>& service_data_values,  // Java Type: byte[]
    const JavaParamRef<jintArray>& manufacturer_data_keys,  // Java Type: int[]
    const JavaParamRef<jobjectArray>&
        manufacturer_data_values  // Java Type: byte[]
    ) {
  std::string device_address = ConvertJavaStringToUTF8(env, address);
  auto iter = devices_.find(device_address);

  bool is_new_device = false;
  std::unique_ptr<BluetoothDeviceAndroid> device_android_owner;
  BluetoothDeviceAndroid* device_android;

  if (iter == devices_.end()) {
    // New device.
    is_new_device = true;
    device_android_owner =
        BluetoothDeviceAndroid::Create(this, bluetooth_device_wrapper);
    device_android = device_android_owner.get();
  } else {
    // Existing device.
    device_android = static_cast<BluetoothDeviceAndroid*>(iter->second.get());
  }
  DCHECK(device_android);

  std::vector<std::string> advertised_uuids_strings;
  AppendJavaStringArrayToStringVector(env, advertised_uuids,
                                      &advertised_uuids_strings);
  BluetoothDevice::UUIDList advertised_bluetooth_uuids;
  for (std::string& uuid : advertised_uuids_strings) {
    advertised_bluetooth_uuids.push_back(BluetoothUUID(std::move(uuid)));
  }

  std::vector<std::string> service_data_keys_vector;
  std::vector<std::string> service_data_values_vector;
  AppendJavaStringArrayToStringVector(env, service_data_keys,
                                      &service_data_keys_vector);
  JavaArrayOfByteArrayToStringVector(env, service_data_values,
                                     &service_data_values_vector);
  BluetoothDeviceAndroid::ServiceDataMap service_data_map;
  for (size_t i = 0; i < service_data_keys_vector.size(); i++) {
    service_data_map.insert(
        {BluetoothUUID(service_data_keys_vector[i]),
         std::vector<uint8_t>(service_data_values_vector[i].begin(),
                              service_data_values_vector[i].end())});
  }

  std::vector<jint> manufacturer_data_keys_vector;
  std::vector<std::string> manufacturer_data_values_vector;
  JavaIntArrayToIntVector(env, manufacturer_data_keys,
                          &manufacturer_data_keys_vector);
  JavaArrayOfByteArrayToStringVector(env, manufacturer_data_values,
                                     &manufacturer_data_values_vector);
  BluetoothDeviceAndroid::ManufacturerDataMap manufacturer_data_map;
  for (size_t i = 0; i < manufacturer_data_keys_vector.size(); i++) {
    manufacturer_data_map.insert(
        {static_cast<uint16_t>(manufacturer_data_keys_vector[i]),
         std::vector<uint8_t>(manufacturer_data_values_vector[i].begin(),
                              manufacturer_data_values_vector[i].end())});
  }

  int8_t clamped_tx_power = BluetoothDevice::ClampPower(tx_power);

  device_android->UpdateAdvertisementData(
      BluetoothDevice::ClampPower(rssi), std::move(advertised_bluetooth_uuids),
      service_data_map, manufacturer_data_map,
      // Android uses INT32_MIN to indicate no Advertised Tx Power.
      // https://developer.android.com/reference/android/bluetooth/le/ScanRecord.html#getTxPowerLevel()
      tx_power == INT32_MIN ? nullptr : &clamped_tx_power);

  if (is_new_device) {
    devices_[device_address] = std::move(device_android_owner);
    for (auto& observer : observers_)
      observer.DeviceAdded(this, device_android);
  } else {
    for (auto& observer : observers_)
      observer.DeviceChanged(this, device_android);
  }
}

BluetoothAdapterAndroid::BluetoothAdapterAndroid() : weak_ptr_factory_(this) {
}

BluetoothAdapterAndroid::~BluetoothAdapterAndroid() {
  Java_ChromeBluetoothAdapter_onBluetoothAdapterAndroidDestruction(
      AttachCurrentThread(), j_adapter_);
}

void BluetoothAdapterAndroid::PurgeTimedOutDevices() {
  RemoveTimedOutDevices();
  if (IsDiscovering()) {
    ui_task_runner_->PostDelayedTask(
        FROM_HERE, base::Bind(&BluetoothAdapterAndroid::PurgeTimedOutDevices,
                              weak_ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(kActivePollInterval));
  } else {
    ui_task_runner_->PostDelayedTask(
        FROM_HERE, base::Bind(&BluetoothAdapterAndroid::RemoveTimedOutDevices,
                              weak_ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(kPassivePollInterval));
  }
}

bool BluetoothAdapterAndroid::SetPoweredImpl(bool powered) {
  return Java_ChromeBluetoothAdapter_setPowered(AttachCurrentThread(),
                                                j_adapter_, powered);
}

void BluetoothAdapterAndroid::AddDiscoverySession(
    BluetoothDiscoveryFilter* discovery_filter,
    const base::Closure& callback,
    const DiscoverySessionErrorCallback& error_callback) {
  // TODO(scheib): Support filters crbug.com/490401
  bool session_added = false;
  if (IsPowered()) {
    if (num_discovery_sessions_ > 0) {
      session_added = true;
    } else if (Java_ChromeBluetoothAdapter_startScan(AttachCurrentThread(),
                                                     j_adapter_)) {
      session_added = true;

      // Using a delayed task in order to give the adapter some time
      // to settle before purging devices.
      ui_task_runner_->PostDelayedTask(
          FROM_HERE, base::Bind(&BluetoothAdapterAndroid::PurgeTimedOutDevices,
                                weak_ptr_factory_.GetWeakPtr()),
          base::TimeDelta::FromMilliseconds(kPurgeDelay));
    }
  } else {
    VLOG(1) << "AddDiscoverySession: Fails: !isPowered";
  }

  if (session_added) {
    num_discovery_sessions_++;
    VLOG(1) << "AddDiscoverySession: Now " << unsigned(num_discovery_sessions_)
            << " sessions.";
    callback.Run();
  } else {
    // TODO(scheib): Eventually wire the SCAN_FAILED result through to here.
    error_callback.Run(UMABluetoothDiscoverySessionOutcome::UNKNOWN);
  }
}

void BluetoothAdapterAndroid::RemoveDiscoverySession(
    BluetoothDiscoveryFilter* discovery_filter,
    const base::Closure& callback,
    const DiscoverySessionErrorCallback& error_callback) {
  bool session_removed = false;
  if (num_discovery_sessions_ == 0) {
    VLOG(1) << "RemoveDiscoverySession: No scan in progress.";
    NOTREACHED();
  } else {
    --num_discovery_sessions_;
    session_removed = true;
    if (num_discovery_sessions_ == 0) {
      VLOG(1) << "RemoveDiscoverySession: Now 0 sessions. Stopping scan.";
      session_removed = Java_ChromeBluetoothAdapter_stopScan(
          AttachCurrentThread(), j_adapter_);
      for (const auto& device_id_object_pair : devices_) {
        device_id_object_pair.second->ClearAdvertisementData();
      }
    } else {
      VLOG(1) << "RemoveDiscoverySession: Now "
              << unsigned(num_discovery_sessions_) << " sessions.";
    }
  }

  if (session_removed) {
    callback.Run();
  } else {
    // TODO(scheib): Eventually wire the SCAN_FAILED result through to here.
    error_callback.Run(UMABluetoothDiscoverySessionOutcome::UNKNOWN);
  }
}

void BluetoothAdapterAndroid::SetDiscoveryFilter(
    std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter,
    const base::Closure& callback,
    const DiscoverySessionErrorCallback& error_callback) {
  // TODO(scheib): Support filters crbug.com/490401
  NOTIMPLEMENTED();
  error_callback.Run(UMABluetoothDiscoverySessionOutcome::NOT_IMPLEMENTED);
}

void BluetoothAdapterAndroid::RemovePairingDelegateInternal(
    device::BluetoothDevice::PairingDelegate* pairing_delegate) {
}

}  // namespace device
