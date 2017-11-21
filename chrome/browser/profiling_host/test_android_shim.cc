// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiling_host/test_android_shim.h"

#include "base/android/jni_string.h"
#include "chrome/browser/profiling_host/profiling_process_host.h"
#include "chrome/browser/profiling_host/profiling_test_driver.h"
#include "jni/TestAndroidShim_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

static jlong JNI_TestAndroidShim_Init(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj) {
  TestAndroidShim* profiler = new TestAndroidShim(env, obj);
  return reinterpret_cast<intptr_t>(profiler);
}

TestAndroidShim::TestAndroidShim(JNIEnv* env, jobject obj) {}

TestAndroidShim::~TestAndroidShim() {}

void TestAndroidShim::Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  delete this;
}

jboolean TestAndroidShim::RunTestForMode(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jstring>& mode) {
  profiling::ProfilingTestDriver driver;
  profiling::ProfilingTestDriver::Options options;
  options.mode = profiling::ProfilingProcessHost::ConvertStringToMode(
      base::android::ConvertJavaStringToUTF8(mode));
  options.profiling_already_started = true;
  return driver.RunTest(options);
}
