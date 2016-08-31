// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/translate_infobar.h"

#include <stddef.h>

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#include "jni/TranslateInfoBar_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

// ChromeTranslateClient
// ----------------------------------------------------------

std::unique_ptr<infobars::InfoBar> ChromeTranslateClient::CreateInfoBar(
    std::unique_ptr<translate::TranslateInfoBarDelegate> delegate) const {
  return base::MakeUnique<TranslateInfoBar>(std::move(delegate));
}


// TranslateInfoBar -----------------------------------------------------------

TranslateInfoBar::TranslateInfoBar(
    std::unique_ptr<translate::TranslateInfoBarDelegate> delegate)
    : InfoBarAndroid(std::move(delegate)) {}

TranslateInfoBar::~TranslateInfoBar() {
}

ScopedJavaLocalRef<jobject> TranslateInfoBar::CreateRenderInfoBar(JNIEnv* env) {
  translate::TranslateInfoBarDelegate* delegate = GetDelegate();
  std::vector<base::string16> languages;
  std::vector<std::string> codes;
  languages.reserve(delegate->num_languages());
  codes.reserve(delegate->num_languages());
  for (size_t i = 0; i < delegate->num_languages(); ++i) {
    languages.push_back(delegate->language_name_at(i));
    codes.push_back(delegate->language_code_at(i));
  }
  DCHECK(codes.size() == languages.size());
  base::android::ScopedJavaLocalRef<jobjectArray> java_languages =
      base::android::ToJavaArrayOfStrings(env, languages);
  base::android::ScopedJavaLocalRef<jobjectArray> java_codes =
      base::android::ToJavaArrayOfStrings(env, codes);

  ScopedJavaLocalRef<jstring> source_language_code =
      base::android::ConvertUTF8ToJavaString(
          env, delegate->original_language_code());

  ScopedJavaLocalRef<jstring> target_language_code =
      base::android::ConvertUTF8ToJavaString(env,
                                             delegate->target_language_code());

  return Java_TranslateInfoBar_show(
      env, delegate->translate_step(), source_language_code,
      target_language_code, delegate->ShouldAlwaysTranslate(),
      ShouldDisplayNeverTranslateInfoBarOnCancel(),
      delegate->triggered_from_menu(), java_languages, java_codes);
}

void TranslateInfoBar::ProcessButton(int action) {
  if (!owner())
    return;  // We're closing; don't call anything, it might access the owner.

  translate::TranslateInfoBarDelegate* delegate = GetDelegate();
  if (action == InfoBarAndroid::ACTION_TRANSLATE) {
    delegate->Translate();
    return;
  }

  if (action == InfoBarAndroid::ACTION_CANCEL)
    delegate->TranslationDeclined();
  else if (action == InfoBarAndroid::ACTION_TRANSLATE_SHOW_ORIGINAL)
    delegate->RevertTranslation();
  else
    DCHECK_EQ(InfoBarAndroid::ACTION_NONE, action);

  RemoveSelf();
}

void TranslateInfoBar::PassJavaInfoBar(InfoBarAndroid* source) {
  translate::TranslateInfoBarDelegate* delegate = GetDelegate();
  DCHECK_NE(translate::TRANSLATE_STEP_BEFORE_TRANSLATE,
            delegate->translate_step());

  // Ask the former bar to transfer ownership to us.
  DCHECK(source != NULL);
  static_cast<TranslateInfoBar*>(source)->TransferOwnership(
      this, delegate->translate_step());
}

void TranslateInfoBar::SetJavaInfoBar(
    const base::android::JavaRef<jobject>& java_info_bar) {
  InfoBarAndroid::SetJavaInfoBar(java_info_bar);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_TranslateInfoBar_setNativePtr(env, java_info_bar,
                                     reinterpret_cast<intptr_t>(this));
}

void TranslateInfoBar::ApplyTranslateOptions(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& source_language_code,
    const JavaParamRef<jstring>& target_language_code,
    bool always_translate,
    bool never_translate_language,
    bool never_translate_site) {
  DCHECK(env);
  std::string source_code =
      base::android::ConvertJavaStringToUTF8(env, source_language_code);
  std::string target_code =
      base::android::ConvertJavaStringToUTF8(env, target_language_code);
  translate::TranslateInfoBarDelegate* delegate = GetDelegate();
  delegate->UpdateOriginalLanguage(source_code);
  delegate->UpdateTargetLanguage(target_code);

  if (delegate->ShouldAlwaysTranslate() != always_translate)
    delegate->ToggleAlwaysTranslate();

  if (never_translate_language && delegate->IsTranslatableLanguageByPrefs())
    delegate->ToggleTranslatableLanguageByPrefs();

  if (never_translate_site && !delegate->IsSiteBlacklisted())
    delegate->ToggleSiteBlacklist();
}

void TranslateInfoBar::TransferOwnership(TranslateInfoBar* destination,
                                         translate::TranslateStep new_type) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_TranslateInfoBar_changeTranslateInfoBarType(env, GetJavaInfoBar(),
                                                   new_type);
  ReassignJavaInfoBar(destination);
}

bool TranslateInfoBar::ShouldDisplayNeverTranslateInfoBarOnCancel() {
  translate::TranslateInfoBarDelegate* delegate = GetDelegate();
  return (delegate->translate_step() ==
          translate::TRANSLATE_STEP_BEFORE_TRANSLATE) &&
         delegate->ShouldShowNeverTranslateShortcut();
}

translate::TranslateInfoBarDelegate* TranslateInfoBar::GetDelegate() {
  return delegate()->AsTranslateInfoBarDelegate();
}


// Native JNI methods ---------------------------------------------------------

bool RegisterTranslateInfoBarDelegate(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
