// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILING_HOST_TEST_ANDROID_SHIM_H_
#define CHROME_BROWSER_PROFILING_HOST_TEST_ANDROID_SHIM_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"

// This class implements the native methods of TestAndroidShim.java,
// and acts as a bridge to ProfilingProcessHost. Note that this class is only
// used for testing.
class TestAndroidShim {
 public:
  TestAndroidShim(JNIEnv* env, jobject obj);
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  jboolean RunTestForMode(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj,
                          const base::android::JavaParamRef<jstring>&);

 private:
  ~TestAndroidShim();

  DISALLOW_COPY_AND_ASSIGN(TestAndroidShim);
};

#endif  // CHROME_BROWSER_PROFILING_HOST_TEST_ANDROID_SHIM_H_
