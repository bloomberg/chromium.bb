// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_endpoint_android.h"

#include "jni/ChromeUsbEndpoint_jni.h"

namespace device {

// static
bool UsbEndpointAndroid::RegisterJNI(JNIEnv* env) {
  return RegisterNativesImpl(env);  // Generated in ChromeUsbEndpoint_jni.h
}

// static
UsbEndpointDescriptor UsbEndpointAndroid::Convert(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& usb_endpoint) {
  base::android::ScopedJavaLocalRef<jobject> wrapper =
      Java_ChromeUsbEndpoint_create(env, usb_endpoint.obj());

  UsbEndpointDescriptor endpoint;
  endpoint.address = Java_ChromeUsbEndpoint_getAddress(env, wrapper.obj());
  endpoint.direction = static_cast<UsbEndpointDirection>(
      Java_ChromeUsbEndpoint_getDirection(env, wrapper.obj()));
  endpoint.maximum_packet_size =
      Java_ChromeUsbEndpoint_getMaxPacketSize(env, wrapper.obj());
  jint attributes = Java_ChromeUsbEndpoint_getAttributes(env, wrapper.obj());
  endpoint.synchronization_type =
      static_cast<UsbSynchronizationType>((attributes >> 2) & 3);
  endpoint.usage_type = static_cast<UsbUsageType>((attributes >> 4) & 3);
  endpoint.transfer_type = static_cast<UsbTransferType>(
      Java_ChromeUsbEndpoint_getType(env, wrapper.obj()));
  endpoint.polling_interval =
      Java_ChromeUsbEndpoint_getInterval(env, wrapper.obj());

  return endpoint;
}

}  // namespace device
