// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/android/javatests/core_test.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "jni/CoreTest_jni.h"

namespace mojo {
namespace android {

static void InitApplicationContext(JNIEnv* env,
                                   jobject jcaller,
                                   jobject context) {
  base::android::ScopedJavaLocalRef<jobject> scoped_context(env, context);
  base::android::InitApplicationContext(env, scoped_context);
}

bool RegisterCoreTest(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android
}  // namespace mojo
