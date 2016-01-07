// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_USB_ANDROID_USB_JNI_REGISTRAR_H_
#define DEVICE_USB_ANDROID_USB_JNI_REGISTRAR_H_

#include <jni.h>

namespace device {
namespace android {

// Registers C++ methods in device/usb classes with JNI.
// See https://www.chromium.org/developers/design-documents/android-jni
//
// Must be called before classes in the USB module are used.
bool RegisterUsbJni(JNIEnv* env);

}  // namespace android
}  // namespace device

#endif  // DEVICE_USB_ANDROID_USB_JNI_REGISTRAR_H_
