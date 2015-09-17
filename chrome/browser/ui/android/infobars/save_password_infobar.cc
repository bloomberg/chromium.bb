// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/save_password_infobar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "jni/SavePasswordInfoBar_jni.h"

SavePasswordInfoBar::SavePasswordInfoBar(
    scoped_ptr<SavePasswordInfoBarDelegate> delegate)
    : ConfirmInfoBar(delegate.Pass()) {
}

SavePasswordInfoBar::~SavePasswordInfoBar() {
}

base::android::ScopedJavaLocalRef<jobject>
SavePasswordInfoBar::CreateRenderInfoBar(JNIEnv* env) {
  using base::android::ConvertUTF16ToJavaString;
  using base::android::ScopedJavaLocalRef;
  SavePasswordInfoBarDelegate* save_password_delegate =
      static_cast<SavePasswordInfoBarDelegate*>(delegate());
  ScopedJavaLocalRef<jstring> ok_button_text = ConvertUTF16ToJavaString(
      env, GetTextFor(ConfirmInfoBarDelegate::BUTTON_OK));
  ScopedJavaLocalRef<jstring> cancel_button_text = ConvertUTF16ToJavaString(
      env, GetTextFor(ConfirmInfoBarDelegate::BUTTON_CANCEL));
  ScopedJavaLocalRef<jstring> message_text = ConvertUTF16ToJavaString(
      env, save_password_delegate->GetMessageText());
  ScopedJavaLocalRef<jstring> first_run_experience_message =
      ConvertUTF16ToJavaString(
          env, save_password_delegate->GetFirstRunExperienceMessage());

  return Java_SavePasswordInfoBar_show(
      env, GetEnumeratedIconId(), message_text.obj(),
      save_password_delegate->message_link_range().start(),
      save_password_delegate->message_link_range().end(), ok_button_text.obj(),
      cancel_button_text.obj(), first_run_experience_message.obj());
}

void SavePasswordInfoBar::OnLinkClicked(JNIEnv* env, jobject obj) {
  GetDelegate()->LinkClicked(NEW_FOREGROUND_TAB);
}

bool SavePasswordInfoBar::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

scoped_ptr<infobars::InfoBar> CreateSavePasswordInfoBar(
    scoped_ptr<SavePasswordInfoBarDelegate> delegate) {
  return make_scoped_ptr(new SavePasswordInfoBar(delegate.Pass()));
}
