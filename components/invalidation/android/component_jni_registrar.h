// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_ANDROID_COMPONENT_JNI_REGISTRAR_H_
#define COMPONENTS_INVALIDATION_ANDROID_COMPONENT_JNI_REGISTRAR_H_

#include <jni.h>

namespace invalidation {

namespace android {

// Register all JNI bindings necessary for the invalidation component.
bool RegisterInvalidationJni(JNIEnv* env);

}  // namespace android

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_ANDROID_COMPONENT_JNI_REGISTRAR_H_
