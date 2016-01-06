// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_interface_android.h"

#include "device/usb/usb_endpoint_android.h"
#include "jni/ChromeUsbInterface_jni.h"

using base::android::ScopedJavaLocalRef;

namespace device {

// static
bool UsbInterfaceAndroid::RegisterJNI(JNIEnv* env) {
  return RegisterNativesImpl(env);  // Generated in ChromeUsbInterface_jni.h
}

// static
UsbInterfaceDescriptor UsbInterfaceAndroid::Convert(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& usb_interface) {
  ScopedJavaLocalRef<jobject> wrapper =
      Java_ChromeUsbInterface_create(env, usb_interface.obj());

  UsbInterfaceDescriptor interface;
  interface.interface_number =
      Java_ChromeUsbInterface_getInterfaceNumber(env, wrapper.obj());
  interface.alternate_setting =
      Java_ChromeUsbInterface_getAlternateSetting(env, wrapper.obj());
  interface.interface_class =
      Java_ChromeUsbInterface_getInterfaceClass(env, wrapper.obj());
  interface.interface_subclass =
      Java_ChromeUsbInterface_getInterfaceSubclass(env, wrapper.obj());
  interface.interface_protocol =
      Java_ChromeUsbInterface_getInterfaceProtocol(env, wrapper.obj());

  ScopedJavaLocalRef<jobjectArray> endpoints =
      Java_ChromeUsbInterface_getEndpoints(env, wrapper.obj());
  jsize count = env->GetArrayLength(endpoints.obj());
  interface.endpoints.reserve(count);
  for (jsize i = 0; i < count; ++i) {
    ScopedJavaLocalRef<jobject> endpoint(
        env, env->GetObjectArrayElement(endpoints.obj(), i));
    interface.endpoints.push_back(UsbEndpointAndroid::Convert(env, endpoint));
  }

  return interface;
}

}  // namespace device
