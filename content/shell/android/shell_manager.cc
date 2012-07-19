// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/android/shell_manager.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/lazy_instance.h"
#include "jni/shell_manager_jni.h"

using base::android::ScopedJavaLocalRef;

base::LazyInstance<base::android::ScopedJavaGlobalRef<jobject> >
    g_content_shell_manager = LAZY_INSTANCE_INITIALIZER;

namespace content {

jobject CreateShellView() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_ShellManager_createShell(
      env, g_content_shell_manager.Get().obj()).Release();
}

// Register native methods
bool RegisterShellManager(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

static void Init(JNIEnv* env, jclass clazz, jobject obj) {
  g_content_shell_manager.Get().Reset(
      base::android::ScopedJavaLocalRef<jobject>(env, obj));
}

}  // namespace content
