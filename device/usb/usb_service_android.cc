// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_service_android.h"

#include <string>
#include <vector>

#include "base/android/context_utils.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/device_event_log/device_event_log.h"
#include "device/usb/usb_device_android.h"
#include "jni/ChromeUsbService_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace device {

// static
bool UsbServiceAndroid::RegisterJNI(JNIEnv* env) {
  return RegisterNativesImpl(env);  // Generated in ChromeUsbService_jni.h
}

UsbServiceAndroid::UsbServiceAndroid() {
  JNIEnv* env = AttachCurrentThread();
  j_object_.Reset(
      Java_ChromeUsbService_create(env, base::android::GetApplicationContext(),
                                   reinterpret_cast<jlong>(this)));
  ScopedJavaLocalRef<jobjectArray> devices =
      Java_ChromeUsbService_getDevices(env, j_object_.obj());
  jsize length = env->GetArrayLength(devices.obj());
  for (jsize i = 0; i < length; ++i) {
    ScopedJavaLocalRef<jobject> usb_device(
        env, env->GetObjectArrayElement(devices.obj(), i));
    scoped_refptr<UsbDeviceAndroid> device(
        UsbDeviceAndroid::Create(env, usb_device));
    AddDevice(device);
  }
}

UsbServiceAndroid::~UsbServiceAndroid() {
  JNIEnv* env = AttachCurrentThread();
  Java_ChromeUsbService_close(env, j_object_.obj());
}

scoped_refptr<UsbDevice> UsbServiceAndroid::GetDevice(const std::string& guid) {
  auto it = devices_by_guid_.find(guid);
  if (it == devices_by_guid_.end())
    return nullptr;
  return it->second;
}

void UsbServiceAndroid::GetDevices(const GetDevicesCallback& callback) {
  std::vector<scoped_refptr<UsbDevice>> devices;
  devices.reserve(devices_by_guid_.size());
  for (const auto& map_entry : devices_by_guid_)
    devices.push_back(map_entry.second);
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::Bind(callback, devices));
}

void UsbServiceAndroid::DeviceAttached(JNIEnv* env,
                                       const JavaRef<jobject>& caller,
                                       const JavaRef<jobject>& usb_device) {
  scoped_refptr<UsbDeviceAndroid> device(
      UsbDeviceAndroid::Create(env, usb_device));
  AddDevice(device);
  NotifyDeviceAdded(device);
}

void UsbServiceAndroid::DeviceDetached(JNIEnv* env,
                                       const JavaRef<jobject>& caller,
                                       jint device_id) {
  auto it = devices_by_id_.find(device_id);
  if (it == devices_by_id_.end())
    return;

  scoped_refptr<UsbDeviceAndroid> device = it->second;
  devices_by_id_.erase(it);
  devices_by_guid_.erase(device->guid());

  USB_LOG(USER) << "USB device removed: id=" << device->device_id()
                << " guid=" << device->guid();

  NotifyDeviceRemoved(device);
}

void UsbServiceAndroid::AddDevice(scoped_refptr<UsbDeviceAndroid> device) {
  DCHECK(!ContainsKey(devices_by_id_, device->device_id()));
  DCHECK(!ContainsKey(devices_by_guid_, device->guid()));
  devices_by_id_[device->device_id()] = device;
  devices_by_guid_[device->guid()] = device;

  USB_LOG(USER) << "USB device added: id=" << device->device_id()
                << " vendor=" << device->vendor_id() << " \""
                << device->manufacturer_string()
                << "\", product=" << device->product_id() << " \""
                << device->product_string() << "\", serial=\""
                << device->serial_number() << "\", guid=" << device->guid();
}

}  // namespace device
