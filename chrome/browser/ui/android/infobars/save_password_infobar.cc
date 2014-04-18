// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/save_password_infobar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "chrome/browser/android/resource_mapper.h"
#include "jni/SavePasswordInfoBarDelegate_jni.h"

// SavePasswordInfoBarDelegate-------------------------------------------------

// static
scoped_ptr<infobars::InfoBar> SavePasswordInfoBarDelegate::CreateInfoBar(
    scoped_ptr<SavePasswordInfoBarDelegate> delegate) {
  return scoped_ptr<infobars::InfoBar>(
      new SavePasswordInfoBar(delegate.Pass()));
}

// SavePasswordInfoBar --------------------------------------------------------

SavePasswordInfoBar::SavePasswordInfoBar(
    scoped_ptr<SavePasswordInfoBarDelegate> delegate)
    : ConfirmInfoBar(delegate.PassAs<ConfirmInfoBarDelegate>()),
      java_save_password_delegate_() {
}

SavePasswordInfoBar::~SavePasswordInfoBar() {
}

void SavePasswordInfoBar::SetUseAdditionalAuthentication(
    JNIEnv* env,
    jobject obj,
    bool use_additional_authentication) {
  GetDelegate()->SetUseAdditionalPasswordAuthentication(
      use_additional_authentication);
}

base::android::ScopedJavaLocalRef<jobject>
    SavePasswordInfoBar::CreateRenderInfoBar(JNIEnv* env) {
  java_save_password_delegate_.Reset(
      Java_SavePasswordInfoBarDelegate_create(env));
  base::android::ScopedJavaLocalRef<jstring> ok_button_text =
      base::android::ConvertUTF16ToJavaString(
          env, GetTextFor(ConfirmInfoBarDelegate::BUTTON_OK));
  base::android::ScopedJavaLocalRef<jstring> cancel_button_text =
      base::android::ConvertUTF16ToJavaString(
          env, GetTextFor(ConfirmInfoBarDelegate::BUTTON_CANCEL));
  SavePasswordInfoBarDelegate* delegate = GetDelegate();
  base::android::ScopedJavaLocalRef<jstring> message_text =
      base::android::ConvertUTF16ToJavaString(
          env, reinterpret_cast<ConfirmInfoBarDelegate*>(
              delegate)->GetMessageText());

  return Java_SavePasswordInfoBarDelegate_showSavePasswordInfoBar(
      env,
      java_save_password_delegate_.obj(),
      reinterpret_cast<intptr_t>(this),
      GetEnumeratedIconId(),
      message_text.obj(),
      ok_button_text.obj(),
      cancel_button_text.obj());
}

SavePasswordInfoBarDelegate* SavePasswordInfoBar::GetDelegate() {
  return static_cast<SavePasswordInfoBarDelegate*>(delegate());
}


// Native JNI methods ---------------------------------------------------------

bool RegisterSavePasswordInfoBar(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
