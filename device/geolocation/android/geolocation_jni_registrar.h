// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GEOLOCATION_ANDROID_GEOLOCATION_JNI_REGISTRAR_H_
#define DEVICE_GEOLOCATION_ANDROID_GEOLOCATION_JNI_REGISTRAR_H_

#include <jni.h>

#include "device/geolocation/geolocation_export.h"

namespace device {
namespace android {

// Registers C++ methods in device/geolocation classes with JNI.
// See https://www.chromium.org/developers/design-documents/android-jni
//
// Must be called before classes in the geolocation module are used.
bool DEVICE_GEOLOCATION_EXPORT RegisterGeolocationJni(JNIEnv* env);

}  // namespace android
}  // namespace device

#endif  // DEVICE_GEOLOCATION_ANDROID_GEOLOCATION_JNI_REGISTRAR_H_
