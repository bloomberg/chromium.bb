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

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

// static
InfoBar* TranslateInfoBarDelegate::CreateInfoBar(InfoBarService* owner) {
  return new TranslateInfoBar(owner, this);
}

TranslateInfoBar::TranslateInfoBar(InfoBarService* owner,
                                   TranslateInfoBarDelegate* delegate)
    : InfoBarAndroid(owner, delegate),
      delegate_(delegate),
      java_translate_delegate_() {}

TranslateInfoBar::~TranslateInfoBar() {
}

ScopedJavaLocalRef<jobject> TranslateInfoBar::CreateRenderInfoBar(JNIEnv* env) {
  java_translate_delegate_.Reset(Java_TranslateInfoBarDelegate_create(env));
  ScopedJavaLocalRef<jobject> java_infobar;
  std::vector<string16> languages(delegate_->num_languages());
  for (size_t i = 0; i < delegate_->num_languages(); ++i) {
    languages[i] = delegate_->language_name_at(i);
  }
  ScopedJavaLocalRef<jobjectArray> java_languages =
      base::android::ToJavaArrayOfStrings(env, languages);

  java_infobar = Java_TranslateInfoBarDelegate_showTranslateInfoBar(
      env,
      java_translate_delegate_.obj(),
      reinterpret_cast<jint>(this),
      delegate_->infobar_type(),
      delegate_->original_language_index(),
      delegate_->target_language_index(),
      delegate_->ShouldAlwaysTranslate(),
      ShouldDisplayNeverInfoBarOnNope(),
      java_languages.obj());

  return java_infobar;
}

void TranslateInfoBar::ProcessButton(
    int action, const std::string& action_value) {
  if (action == InfoBarAndroid::ACTION_TRANSLATE) {
    delegate_->Translate();
  } else if (action == InfoBarAndroid::ACTION_CANCEL) {
    delegate_->TranslationDeclined();
  } else if (action == InfoBarAndroid::ACTION_TRANSLATE_SHOW_ORIGINAL) {
    delegate_->RevertTranslation();
  } else if (action != InfoBarAndroid::ACTION_NONE) {
    NOTREACHED();
  }

  if (action != InfoBarAndroid::ACTION_TRANSLATE) {
    // do not close the infobar upon translate
    // since it will be replaced by a different one
    // which will close this current infobar.
    CloseInfoBar();
  }
}

void TranslateInfoBar::SetJavaDelegate(jobject delegate) {
  JNIEnv* env = AttachCurrentThread();
  java_translate_delegate_.Reset(env, delegate);
}

void TranslateInfoBar::PassJavaInfoBar(InfoBarAndroid* source) {
  if (delegate_->infobar_type() != TranslateInfoBarDelegate::TRANSLATING &&
      delegate_->infobar_type() != TranslateInfoBarDelegate::AFTER_TRANSLATE &&
      delegate_->infobar_type() !=
          TranslateInfoBarDelegate::TRANSLATION_ERROR) {
    return;
  }
  DCHECK(source != NULL);
  TranslateInfoBar* source_infobar = static_cast<TranslateInfoBar*>(source);

  // Ask the former bar to transfer ownership to us.
  source_infobar->TransferOwnership(this, delegate_->infobar_type());
}

void TranslateInfoBar::TransferOwnership(
    TranslateInfoBar* destination,
    TranslateInfoBarDelegate::Type new_type) {
  JNIEnv* env = AttachCurrentThread();
  if (Java_TranslateInfoBarDelegate_changeTranslateInfoBarTypeAndPointer(
          env,
          java_translate_delegate_.obj(),
          reinterpret_cast<jint>(destination),
          new_type)) {
    ReassignJavaInfoBar(destination);
    destination->SetJavaDelegate(java_translate_delegate_.Release());
  }
}

bool TranslateInfoBar::ShouldDisplayNeverInfoBarOnNope() {
  return
      (delegate_->infobar_type() == TranslateInfoBarDelegate::BEFORE_TRANSLATE)
      && (delegate_->ShouldShowNeverTranslateShortcut());
}

// -----------------------------------------------------------------------------
// Native JNI methods for the translate delegate.
// -----------------------------------------------------------------------------
void TranslateInfoBar::ApplyTranslateOptions(JNIEnv* env, jobject obj,
    int source_language_index, int target_language_index,
    bool always_translate, bool never_translate_language,
    bool never_translate_site) {
  if (delegate_->original_language_index() !=
      static_cast<size_t>(source_language_index))
    delegate_->set_original_language_index(source_language_index);

  if (delegate_->target_language_index() !=
      static_cast<size_t>(target_language_index))
     delegate_->set_target_language_index(target_language_index);

  if (delegate_->ShouldAlwaysTranslate() != always_translate)
    delegate_->ToggleAlwaysTranslate();

  if (never_translate_language && delegate_->IsTranslatableLanguageByPrefs())
    delegate_->ToggleTranslatableLanguageByPrefs();

  if (never_translate_site && !delegate_->IsSiteBlacklisted())
    delegate_->ToggleSiteBlacklist();
}

bool RegisterTranslateInfoBarDelegate(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
