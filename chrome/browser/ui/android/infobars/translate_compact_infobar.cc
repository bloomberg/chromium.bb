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
#include "chrome/browser/translate/android/translate_utils.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#include "jni/TranslateCompactInfoBar_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

// TranslateInfoBar -----------------------------------------------------------

TranslateCompactInfoBar::TranslateCompactInfoBar(
    std::unique_ptr<translate::TranslateInfoBarDelegate> delegate)
    : InfoBarAndroid(std::move(delegate)) {
  // |translate_driver| must already be bound.
  DCHECK(GetDelegate()->GetTranslateDriver());
  translate_driver_ = static_cast<translate::ContentTranslateDriver*>(
      GetDelegate()->GetTranslateDriver());
  translate_driver_->AddObserver(this);
}

TranslateCompactInfoBar::~TranslateCompactInfoBar() {
  DCHECK(translate_driver_);
  translate_driver_->RemoveObserver(this);
}

ScopedJavaLocalRef<jobject> TranslateCompactInfoBar::CreateRenderInfoBar(
    JNIEnv* env) {
  translate::TranslateInfoBarDelegate* delegate = GetDelegate();

  base::android::ScopedJavaLocalRef<jobjectArray> java_languages =
      TranslateUtils::GetJavaLanguages(env, delegate);
  base::android::ScopedJavaLocalRef<jobjectArray> java_codes =
      TranslateUtils::GetJavaLanguageCodes(env, delegate);

  ScopedJavaLocalRef<jstring> source_language_code =
      base::android::ConvertUTF8ToJavaString(
          env, delegate->original_language_code());

  ScopedJavaLocalRef<jstring> target_language_code =
      base::android::ConvertUTF8ToJavaString(env,
                                             delegate->target_language_code());
  return Java_TranslateCompactInfoBar_create(env, source_language_code,
                                             target_language_code,
                                             java_languages, java_codes);
}

void TranslateCompactInfoBar::ProcessButton(int action) {
  if (!owner())
    return;  // We're closing; don't call anything, it might access the owner.

  // TODO(ramyasharma): Handle other button clicks.
  translate::TranslateInfoBarDelegate* delegate = GetDelegate();
  if (action == InfoBarAndroid::ACTION_TRANSLATE)
    delegate->Translate();
  else if (action == InfoBarAndroid::ACTION_TRANSLATE_SHOW_ORIGINAL)
    delegate->RevertTranslation();
  else
    DCHECK_EQ(InfoBarAndroid::ACTION_NONE, action);
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

void TranslateCompactInfoBar::OnPageTranslated(
    const std::string& original_lang,
    const std::string& translated_lang,
    translate::TranslateErrors::Type error_type) {
  if (!owner())
    return;  // We're closing; don't call anything, it might access the owner.

  DCHECK(translate_driver_);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_TranslateCompactInfoBar_onPageTranslated(env, GetJavaInfoBar(),
                                                error_type);
}

translate::TranslateInfoBarDelegate* TranslateCompactInfoBar::GetDelegate() {
  return delegate()->AsTranslateInfoBarDelegate();
}

// Native JNI methods ---------------------------------------------------------

// static
bool RegisterTranslateCompactInfoBar(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
