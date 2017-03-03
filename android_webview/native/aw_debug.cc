// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/aw_debug.h"

#include "android_webview/common/crash_reporter/aw_microdump_crash_reporter.h"
#include "android_webview/common/crash_reporter/crash_keys.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/threading/thread_restrictions.h"
#include "jni/AwDebug_jni.h"

using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace android_webview {

static jboolean DumpWithoutCrashing(JNIEnv* env,
                                    const JavaParamRef<jclass>& clazz,
                                    const JavaParamRef<jstring>& dump_path) {
  // This may be called from any thread, and we might be in a state
  // where it is impossible to post tasks, so we have to be prepared
  // to do IO from this thread.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  base::File target(base::FilePath(ConvertJavaStringToUTF8(env, dump_path)),
                    base::File::FLAG_OPEN_TRUNCATED | base::File::FLAG_READ |
                        base::File::FLAG_WRITE);
  if (!target.IsValid())
    return false;
  // breakpad_linux::HandleCrashDump will close this fd once it is done.
  return crash_reporter::DumpWithoutCrashingToFd(target.TakePlatformFile());
}

static void InitCrashKeysForWebViewTesting(JNIEnv* env,
                                           const JavaParamRef<jclass>& clazz) {
  crash_keys::InitCrashKeysForWebViewTesting();
}

static void SetCrashKeyValue(JNIEnv* env,
                             const JavaParamRef<jclass>& clazz,
                             const JavaParamRef<jstring>& key,
                             const JavaParamRef<jstring>& value) {
  base::debug::SetCrashKeyValue(ConvertJavaStringToUTF8(env, key),
                                ConvertJavaStringToUTF8(env, value));
}

bool RegisterAwDebug(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android_webview
