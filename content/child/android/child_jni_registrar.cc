// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/android/child_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "webkit/child/fling_animator_impl_android.h"

namespace {
base::android::RegistrationMethod kContentRegisteredMethods[] = {
  { "FlingAnimator", webkit_glue::FlingAnimatorImpl::RegisterJni },
};

}  // namespace

namespace content {
namespace android {

bool RegisterChildJni(JNIEnv* env) {
  return RegisterNativeMethods(env, kContentRegisteredMethods,
                               arraysize(kContentRegisteredMethods));
}

}  // namespace android
}  // namespace content
