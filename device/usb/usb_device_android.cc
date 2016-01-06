// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_device_android.h"

#include "base/android/build_info.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/thread_task_runner_handle.h"
#include "device/usb/usb_configuration_android.h"
#include "device/usb/usb_device_handle.h"
#include "device/usb/usb_interface_android.h"
#include "jni/ChromeUsbDevice_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace device {

// static
bool UsbDeviceAndroid::RegisterJNI(JNIEnv* env) {
  return RegisterNativesImpl(env);  // Generated in ChromeUsbDevice_jni.h
}

// static
scoped_refptr<UsbDeviceAndroid> UsbDeviceAndroid::Create(
    JNIEnv* env,
    const JavaRef<jobject>& usb_device) {
  ScopedJavaLocalRef<jobject> wrapper =
      Java_ChromeUsbDevice_create(env, usb_device.obj());
  uint16_t vendor_id = Java_ChromeUsbDevice_getVendorId(env, wrapper.obj());
  uint16_t product_id = Java_ChromeUsbDevice_getProductId(env, wrapper.obj());
  ScopedJavaLocalRef<jstring> manufacturer_string =
      Java_ChromeUsbDevice_getManufacturerName(env, wrapper.obj());
  ScopedJavaLocalRef<jstring> product_string =
      Java_ChromeUsbDevice_getProductName(env, wrapper.obj());
  ScopedJavaLocalRef<jstring> serial_number =
      Java_ChromeUsbDevice_getSerialNumber(env, wrapper.obj());
  return make_scoped_refptr(new UsbDeviceAndroid(
      env, vendor_id, product_id,
      ConvertJavaStringToUTF16(env, manufacturer_string),
      ConvertJavaStringToUTF16(env, product_string),
      ConvertJavaStringToUTF16(env, serial_number), wrapper));
}

void UsbDeviceAndroid::Open(const OpenCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::Bind(callback, nullptr));
}

const UsbConfigDescriptor* UsbDeviceAndroid::GetActiveConfiguration() {
  return nullptr;
}

UsbDeviceAndroid::UsbDeviceAndroid(JNIEnv* env,
                                   uint16_t vendor_id,
                                   uint16_t product_id,
                                   const base::string16& manufacturer_string,
                                   const base::string16& product_string,
                                   const base::string16& serial_number,
                                   const JavaRef<jobject>& wrapper)
    : UsbDevice(vendor_id,
                product_id,
                manufacturer_string,
                product_string,
                serial_number) {
  j_object_.Reset(wrapper);

  if (base::android::BuildInfo::GetInstance()->sdk_int() >= 21) {
    ScopedJavaLocalRef<jobjectArray> configurations =
        Java_ChromeUsbDevice_getConfigurations(env, j_object_.obj());
    jsize count = env->GetArrayLength(configurations.obj());
    configurations_.reserve(count);
    for (jsize i = 0; i < count; ++i) {
      ScopedJavaLocalRef<jobject> config(
          env, env->GetObjectArrayElement(configurations.obj(), i));
      configurations_.push_back(UsbConfigurationAndroid::Convert(env, config));
    }
  } else {
    // Pre-lollipop only the first configuration was supported. Build a basic
    // configuration out of the available interfaces.
    UsbConfigDescriptor config;
    config.configuration_value = 1;  // Reasonable guess.
    config.self_powered = false;     // Arbitrary default.
    config.remote_wakeup = false;    // Arbitrary default.
    config.maximum_power = 0;        // Arbitrary default.

    ScopedJavaLocalRef<jobjectArray> interfaces =
        Java_ChromeUsbDevice_getInterfaces(env, wrapper.obj());
    jsize count = env->GetArrayLength(interfaces.obj());
    config.interfaces.reserve(count);
    for (jsize i = 0; i < count; ++i) {
      ScopedJavaLocalRef<jobject> interface(
          env, env->GetObjectArrayElement(interfaces.obj(), i));
      config.interfaces.push_back(UsbInterfaceAndroid::Convert(env, interface));
    }
    configurations_.push_back(config);
  }
}

UsbDeviceAndroid::~UsbDeviceAndroid() {}

}  // namespace device
