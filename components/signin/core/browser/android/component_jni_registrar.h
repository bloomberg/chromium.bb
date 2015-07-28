// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_ANDROID_COMPONENT_JNI_REGISTRAR_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_ANDROID_COMPONENT_JNI_REGISTRAR_H_

#include <jni.h>

namespace signin {
namespace android {

bool RegisterSigninJni(JNIEnv* env);

}  // namespace android
}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_ANDROID_COMPONENT_JNI_REGISTRAR_H_
