// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_ANDROID_CAST_JNI_REGISTRAR_H_
#define CHROMECAST_ANDROID_CAST_JNI_REGISTRAR_H_

#include <jni.h>

namespace chromecast {
namespace android {

// Register all JNI bindings necessary for the Android cast shell.
bool RegisterJni(JNIEnv* env);

}  // namespace android
}  // namespace chromecast

#endif  // CHROMECAST_ANDROID_CAST_JNI_REGISTRAR_H_
