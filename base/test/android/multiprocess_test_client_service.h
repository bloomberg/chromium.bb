// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_ANDROID_MULTIPROCESS_TEST_CLIENT_SERVICE_H
#define BASE_TEST_ANDROID_MULTIPROCESS_TEST_CLIENT_SERVICE_H

#include "base/android/jni_android.h"

namespace base {
namespace android {

bool RegisterMultiprocessTestClientServiceJni(JNIEnv* env);

}  // namespace android
}  // namespace base

#endif  // BASE_TEST_ANDROID_MULTIPROCESS_TEST_CLIENT_SERVICE_H