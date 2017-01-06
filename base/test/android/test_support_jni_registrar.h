// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_ANDROID_TEST_SUPPORT_JNI_REGISTRAR_H_
#define BASE_TEST_ANDROID_TEST_SUPPORT_JNI_REGISTRAR_H_

#include <jni.h>

#include "base/base_export.h"

namespace base {
namespace android {

// Registers all JNI bindings necessary for base test support.
BASE_EXPORT bool RegisterTestSupportJni(JNIEnv* env);

}  // namespace android
}  // namespace base

#endif  // BASE_TEST_ANDROID_TEST_SUPPORT_JNI_REGISTRAR_H_