// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_ANDROID_SYSTEM_MOJO_JNI_REGISTRAR_H_
#define MOJO_ANDROID_SYSTEM_MOJO_JNI_REGISTRAR_H_

#include <jni.h>

namespace mojo {
namespace android {

// Register all JNI bindings necessary for mojo system.
bool RegisterSystemJni(JNIEnv* env);

}  // namespace android
}  // namespace mojo

#endif  // MOJO_ANDROID_SYSTEM_MOJO_JNI_REGISTRAR_H_
