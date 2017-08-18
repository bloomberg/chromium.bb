// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_ANDROID_JAVATESTS_CORE_TEST_H_
#define MOJO_ANDROID_JAVATESTS_CORE_TEST_H_

#include <jni.h>

#include "base/android/jni_android.h"

namespace mojo {
namespace android {

JNI_EXPORT bool RegisterMojoTestRule(JNIEnv* env);

}  // namespace android
}  // namespace mojo

#endif  // MOJO_SYSTEM_ANDROID_JAVATESTS_CORE_TEST_H_
