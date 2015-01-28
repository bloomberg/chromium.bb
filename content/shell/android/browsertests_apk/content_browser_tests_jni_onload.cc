// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_onload_delegate.h"
#include "content/public/app/content_jni_onload.h"
#include "content/public/app/content_main.h"
#include "content/public/test/nested_message_pump_android.h"
#include "content/shell/android/browsertests_apk/content_browser_tests_android.h"
#include "content/shell/android/shell_jni_registrar.h"
#include "content/shell/app/shell_main_delegate.h"

namespace {

class ContentBrowserTestsJNIOnLoadDelegate :
      public base::android::JNIOnLoadDelegate {
 public:
  bool RegisterJNI(JNIEnv* env) override;
  bool Init() override;
};

bool ContentBrowserTestsJNIOnLoadDelegate::RegisterJNI(JNIEnv* env) {
  return content::android::RegisterShellJni(env) &&
      content::NestedMessagePumpAndroid::RegisterJni(env) &&
      content::RegisterContentBrowserTestsAndroid(env);
}

bool ContentBrowserTestsJNIOnLoadDelegate::Init() {
  content::SetContentMainDelegate(new content::ShellMainDelegate());
  return true;
}

}  // namespace


// This is called by the VM when the shared library is first loaded.
JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  ContentBrowserTestsJNIOnLoadDelegate delegate;
  if (!content::android::OnJNIOnLoad(vm, &delegate))
    return -1;

  return JNI_VERSION_1_4;
}
