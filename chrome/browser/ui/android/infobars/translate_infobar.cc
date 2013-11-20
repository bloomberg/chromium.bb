// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/translate_infobar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_helper.h"
#include "chrome/browser/translate/translate_infobar_delegate.h"
#include "grit/generated_resources.h"
#include "jni/TranslateInfoBarDelegate_jni.h"
#include "ui/base/l10n/l10n_util.h"


// InfoBarDelegateAndroid -----------------------------------------------------

// static
InfoBar* TranslateInfoBarDelegate::CreateInfoBar(InfoBarService* owner) {
  return new TranslateInfoBar(owner, this);
}


// TranslateInfoBar -----------------------------------------------------------

TranslateInfoBar::TranslateInfoBar(InfoBarService* owner,
                                   TranslateInfoBarDelegate* delegate)
    : InfoBarAndroid(owner, delegate),
      delegate_(delegate),
      java_translate_delegate_() {
}

TranslateInfoBar::~TranslateInfoBar() {
}

ScopedJavaLocalRef<jobject> TranslateInfoBar::CreateRenderInfoBar(JNIEnv* env) {
  java_translate_delegate_.Reset(Java_TranslateInfoBarDelegate_create(env));
  std::vector<string16> languages(delegate_->num_languages());
  for (size_t i = 0; i < delegate_->num_languages(); ++i)
    languages[i] = delegate_->language_name_at(i);

  base::android::ScopedJavaLocalRef<jobjectArray> java_languages =
      base::android::ToJavaArrayOfStrings(env, languages);
  return Java_TranslateInfoBarDelegate_showTranslateInfoBar(
      env,
      java_translate_delegate_.obj(),
      reinterpret_cast<jint>(this),
      delegate_->infobar_type(),
      delegate_->original_language_index(),
      delegate_->target_language_index(),
      delegate_->ShouldAlwaysTranslate(),
      ShouldDisplayNeverTranslateInfoBarOnCancel(),
      java_languages.obj());
}

void TranslateInfoBar::ProcessButton(
    int action, const std::string& action_value) {
  if (!owner())
     return; // We're closing; don't call anything, it might access the owner.

  if (action == InfoBarAndroid::ACTION_TRANSLATE) {
    delegate_->Translate();
    return;
  }

  if (action == InfoBarAndroid::ACTION_CANCEL)
    delegate_->TranslationDeclined();
  else if (action == InfoBarAndroid::ACTION_TRANSLATE_SHOW_ORIGINAL)
    delegate_->RevertTranslation();
  else
    DCHECK_EQ(InfoBarAndroid::ACTION_NONE, action);

  RemoveSelf();
}

void TranslateInfoBar::PassJavaInfoBar(InfoBarAndroid* source) {
  DCHECK_NE(
      delegate_->infobar_type(), TranslateInfoBarDelegate::BEFORE_TRANSLATE);

  // Ask the former bar to transfer ownership to us.
  DCHECK(source != NULL);
  static_cast<TranslateInfoBar*>(source)->TransferOwnership(
      this, delegate_->infobar_type());
}

void TranslateInfoBar::ApplyTranslateOptions(JNIEnv* env,
                                             jobject obj,
                                             int source_language_index,
                                             int target_language_index,
                                             bool always_translate,
                                             bool never_translate_language,
                                             bool never_translate_site) {
  delegate_->UpdateOriginalLanguageIndex(source_language_index);
  delegate_->UpdateTargetLanguageIndex(target_language_index);

  if (delegate_->ShouldAlwaysTranslate() != always_translate)
    delegate_->ToggleAlwaysTranslate();

  if (never_translate_language && delegate_->IsTranslatableLanguageByPrefs())
    delegate_->ToggleTranslatableLanguageByPrefs();

  if (never_translate_site && !delegate_->IsSiteBlacklisted())
    delegate_->ToggleSiteBlacklist();
}

void TranslateInfoBar::TransferOwnership(
    TranslateInfoBar* destination,
    TranslateInfoBarDelegate::Type new_type) {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (Java_TranslateInfoBarDelegate_changeTranslateInfoBarTypeAndPointer(
      env, java_translate_delegate_.obj(),
      reinterpret_cast<jint>(destination), new_type)) {
    ReassignJavaInfoBar(destination);
    destination->SetJavaDelegate(java_translate_delegate_.Release());
  }
}

void TranslateInfoBar::SetJavaDelegate(jobject delegate) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_translate_delegate_.Reset(env, delegate);
}

bool TranslateInfoBar::ShouldDisplayNeverTranslateInfoBarOnCancel() {
  return
      (delegate_->infobar_type() ==
          TranslateInfoBarDelegate::BEFORE_TRANSLATE) &&
      (delegate_->ShouldShowNeverTranslateShortcut());
}


// Native JNI methods ---------------------------------------------------------

bool RegisterTranslateInfoBarDelegate(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
