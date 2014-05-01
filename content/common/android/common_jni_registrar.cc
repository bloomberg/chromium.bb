// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/android/common_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "content/common/android/hash_set.h"

namespace {
base::android::RegistrationMethod kContentRegisteredMethods[] = {
  { "HashSet", content::RegisterHashSet },
};

}  // namespace

namespace content {
namespace android {

bool RegisterCommonJni(JNIEnv* env) {
  return RegisterNativeMethods(env, kContentRegisteredMethods,
                               arraysize(kContentRegisteredMethods));
}

}  // namespace android
}  // namespace content
