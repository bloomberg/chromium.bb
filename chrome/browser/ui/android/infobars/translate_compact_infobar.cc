// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/translate_compact_infobar.h"

#include <stddef.h>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "base/memory/ptr_util.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#include "jni/TranslateCompactInfoBar_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

// TranslateInfoBar -----------------------------------------------------------

TranslateCompactInfoBar::TranslateCompactInfoBar(
    std::unique_ptr<translate::TranslateInfoBarDelegate> delegate)
    : InfoBarAndroid(std::move(delegate)) {}

TranslateCompactInfoBar::~TranslateCompactInfoBar() {}

ScopedJavaLocalRef<jobject> TranslateCompactInfoBar::CreateRenderInfoBar(
    JNIEnv* env) {
  // TODO(ramyasharma): Implement.
  return Java_TranslateCompactInfoBar_create(env);
}

void TranslateCompactInfoBar::ProcessButton(int action) {
  // TODO(ramyasharma): Implement.
}

void TranslateCompactInfoBar::SetJavaInfoBar(
    const base::android::JavaRef<jobject>& java_info_bar) {
  InfoBarAndroid::SetJavaInfoBar(java_info_bar);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_TranslateCompactInfoBar_setNativePtr(env, java_info_bar,
                                            reinterpret_cast<intptr_t>(this));
}

void TranslateCompactInfoBar::ApplyTranslateOptions(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  // TODO(ramyasharma): Implement.
}

translate::TranslateInfoBarDelegate* TranslateCompactInfoBar::GetDelegate() {
  return delegate()->AsTranslateInfoBarDelegate();
}

// Native JNI methods ---------------------------------------------------------

// static
bool RegisterTranslateCompactInfoBar(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
