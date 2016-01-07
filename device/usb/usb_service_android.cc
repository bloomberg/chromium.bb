// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_service_android.h"

#include <string>
#include <vector>

#include "base/android/context_utils.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/thread_task_runner_handle.h"
#include "device/usb/usb_device_android.h"
#include "jni/ChromeUsbService_jni.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

namespace device {

// static
bool UsbServiceAndroid::RegisterJNI(JNIEnv* env) {
  return RegisterNativesImpl(env);  // Generated in ChromeUsbService_jni.h
}

UsbServiceAndroid::UsbServiceAndroid() {
  j_object_.Reset(Java_ChromeUsbService_create(
      AttachCurrentThread(), base::android::GetApplicationContext()));
}

UsbServiceAndroid::~UsbServiceAndroid() {}

scoped_refptr<UsbDevice> UsbServiceAndroid::GetDevice(const std::string& guid) {
  return nullptr;
}

void UsbServiceAndroid::GetDevices(const GetDevicesCallback& callback) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> devices =
      Java_ChromeUsbService_getDevices(env, j_object_.obj());
  jsize length = env->GetArrayLength(devices.obj());

  std::vector<scoped_refptr<UsbDevice>> results;
  for (jsize i = 0; i < length; ++i) {
    ScopedJavaLocalRef<jobject> raw_device(
        env, env->GetObjectArrayElement(devices.obj(), i));
    results.push_back(UsbDeviceAndroid::Create(env, raw_device));
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::Bind(callback, results));
}

}  // namespace device
