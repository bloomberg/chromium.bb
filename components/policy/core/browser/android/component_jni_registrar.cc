// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/android/component_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/basictypes.h"
#include "components/policy/core/browser/android/android_combined_policy_provider.h"
#include "components/policy/core/browser/android/policy_converter.h"

namespace policy {
namespace android {

static base::android::RegistrationMethod kPolicyRegisteredMethods[] = {
    {"PolicyConverter", PolicyConverter::Register},
    {"AndroidCombinedPolicyProvider", AndroidCombinedPolicyProvider::Register},
};

bool RegisterPolicy(JNIEnv* env) {
  return base::android::RegisterNativeMethods(
      env, kPolicyRegisteredMethods, arraysize(kPolicyRegisteredMethods));
}

}  // namespace android
}  // namespace policy
