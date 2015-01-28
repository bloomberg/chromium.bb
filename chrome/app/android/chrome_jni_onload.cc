// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_onload_delegate.h"
#include "chrome/app/android/chrome_android_initializer.h"
#include "content/public/app/content_jni_onload.h"

namespace {

class ChromeJNIOnLoadDelegate : public base::android::JNIOnLoadDelegate {
 public:
  bool RegisterJNI(JNIEnv* env) override;
  bool Init() override;
};

bool ChromeJNIOnLoadDelegate::RegisterJNI(JNIEnv* env) {
  return true;
}

bool ChromeJNIOnLoadDelegate::Init() {
  // TODO(michaelbai): Move the JNI registration from RunChrome() to
  // RegisterJNI().
  return RunChrome();
}

}  // namespace


// This is called by the VM when the shared library is first loaded.
JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  ChromeJNIOnLoadDelegate delegate;
  if (!content::android::OnJNIOnLoad(vm, &delegate))
    return -1;

  return JNI_VERSION_1_4;
}
