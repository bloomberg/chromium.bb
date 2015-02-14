// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_onload_delegate.h"
#include "content/public/app/content_jni_onload.h"
#include "content/public/app/content_main.h"
#include "content/public/browser/android/compositor.h"
#include "content/shell/android/linker_test_apk/chromium_linker_test_linker_tests.h"
#include "content/shell/android/shell_jni_registrar.h"
#include "content/shell/app/shell_main_delegate.h"

namespace {

class ChromiumLinkerTestJNIOnLoadDelegate :
    public base::android::JNIOnLoadDelegate {
 public:
  bool RegisterJNI(JNIEnv* env) override {
    // To be called only from the UI thread.  If loading the library is done on
    // a separate thread, this should be moved elsewhere.
    if (!content::android::RegisterShellJni(env))
      return false;

    if (!content::RegisterLinkerTestsJni(env))
      return false;

    return true;
  }

  bool Init() override {
    content::Compositor::Initialize();
    content::SetContentMainDelegate(new content::ShellMainDelegate());
    return true;
  }
};

}  // namespace

// This is called by the VM when the shared library is first loaded.
JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  ChromiumLinkerTestJNIOnLoadDelegate delegate;
  if (!content::android::OnJNIOnLoad(vm, &delegate))
    return -1;

  return JNI_VERSION_1_4;
}
