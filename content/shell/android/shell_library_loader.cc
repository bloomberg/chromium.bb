// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/bind.h"
#include "content/public/app/content_jni_onload.h"
#include "content/public/app/content_main.h"
#include "content/public/browser/android/compositor.h"
#include "content/shell/android/shell_jni_registrar.h"
#include "content/shell/app/shell_main_delegate.h"

namespace {

bool RegisterJNI(JNIEnv* env) {
  return content::android::RegisterShellJni(env);
}

bool Init() {
  content::Compositor::Initialize();
  content::SetContentMainDelegate(new content::ShellMainDelegate());
  return true;
}

}  // namespace


// This is called by the VM when the shared library is first loaded.
JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  if (!content::android::OnJNIOnLoadRegisterJNI(
          vm, base::Bind(&RegisterJNI)) ||
      !content::android::OnJNIOnLoadInit(base::Bind(&Init)))
    return -1;

  return JNI_VERSION_1_4;
}
