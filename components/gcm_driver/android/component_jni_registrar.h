// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_ANDROID_COMPONENT_JNI_REGISTRAR_H_
#define COMPONENTS_GCM_DRIVER_ANDROID_COMPONENT_JNI_REGISTRAR_H_

#include <jni.h>

namespace gcm {

namespace android {

// Register all JNI bindings necessary for the gcm_driver component.
bool RegisterGCMDriverJni(JNIEnv* env);

}  // namespace android

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_ANDROID_COMPONENT_JNI_REGISTRAR_H_
