// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/android/shell_view.h"

#include "base/android/jni_android.h"
#include "jni/shell_view_jni.h"

namespace content {

ShellView::ShellView(JNIEnv* env, jobject obj)
    : weak_java_shell_view_(env, obj) {
}

ShellView::~ShellView() {
}

void ShellView::InitFromTabContents(WebContents* tab_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ShellView_initFromNativeTabContents(
      env, weak_java_shell_view_.get(env).obj(),
      reinterpret_cast<jint>(tab_contents));
}

// static
bool ShellView::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace content

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

static jint Init(JNIEnv* env, jobject obj) {
  content::ShellView* shell_view = new content::ShellView(env, obj);
  return reinterpret_cast<jint>(shell_view);
}
