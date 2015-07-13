// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_JSON_ANDROID_COMPONENT_JNI_REGISTRAR_H_
#define COMPONENTS_SAFE_JSON_ANDROID_COMPONENT_JNI_REGISTRAR_H_

#include <jni.h>

namespace safe_json {
namespace android {

// Register all JNI bindings necessary for the safe_json component.
bool RegisterSafeJsonJni(JNIEnv* env);

}  // namespace android
}  // namespace safe_json

#endif  // COMPONENTS_SAFE_JSON_ANDROID_COMPONENT_JNI_REGISTRAR_H_
