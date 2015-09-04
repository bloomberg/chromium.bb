// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/java_exception_reporter.h"

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/debug/dump_without_crashing.h"
#include "jni/JavaExceptionReporter_jni.h"

namespace chrome {
namespace android {

void InitJavaExceptionReporter() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_JavaExceptionReporter_installHandler(env);
}

void ReportJavaException(JNIEnv* env,
                         const JavaParamRef<jclass>& jcaller,
                         const JavaParamRef<jthrowable>& e) {
  // Set the exception_string in BuildInfo so that breakpad can read it.
  base::android::BuildInfo::GetInstance()->SetJavaExceptionInfo(
      base::android::GetJavaExceptionInfo(env, e));
  base::debug::DumpWithoutCrashing();
  base::android::BuildInfo::GetInstance()->ClearJavaExceptionInfo();
}

bool RegisterJavaExceptionReporterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}


}  // namespace android
}  // namespace chrome


