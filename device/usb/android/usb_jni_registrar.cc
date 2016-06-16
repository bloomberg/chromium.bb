// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/android/usb_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "device/usb/usb_configuration_android.h"
#include "device/usb/usb_device_android.h"
#include "device/usb/usb_device_handle_android.h"
#include "device/usb/usb_endpoint_android.h"
#include "device/usb/usb_interface_android.h"
#include "device/usb/usb_service_android.h"

namespace device {
namespace android {
namespace {

const base::android::RegistrationMethod kRegisteredMethods[] = {
    {"UsbConfigurationAndroid", device::UsbConfigurationAndroid::RegisterJNI},
    {"UsbDeviceAndroid", device::UsbDeviceAndroid::RegisterJNI},
    {"UsbDeviceHandleAndroid", device::UsbDeviceHandleAndroid::RegisterJNI},
    {"UsbEndpointAndroid", device::UsbEndpointAndroid::RegisterJNI},
    {"UsbInterfaceAndroid", device::UsbInterfaceAndroid::RegisterJNI},
    {"UsbServiceAndroid", device::UsbServiceAndroid::RegisterJNI},
};

}  // namespace

bool RegisterUsbJni(JNIEnv* env) {
  return RegisterNativeMethods(env, kRegisteredMethods,
                               arraysize(kRegisteredMethods));
}

}  // namespace android
}  // namespace device
