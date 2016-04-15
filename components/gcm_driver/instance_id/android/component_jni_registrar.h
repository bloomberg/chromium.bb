// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_INSTANCE_ID_ANDROID_COMPONENT_JNI_REGISTRAR_H_
#define COMPONENTS_GCM_DRIVER_INSTANCE_ID_ANDROID_COMPONENT_JNI_REGISTRAR_H_

#include <jni.h>

namespace instance_id {
namespace android {

// Register all JNI bindings necessary for the gcm_driver/instance_id component.
bool RegisterInstanceIDJni(JNIEnv* env);

}  // namespace android
}  // namespace instance_id

#endif  // COMPONENTS_GCM_DRIVER_INSTANCE_ID_ANDROID_COMPONENT_JNI_REGISTRAR_H_
