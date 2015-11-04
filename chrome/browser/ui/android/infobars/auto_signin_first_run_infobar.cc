// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/auto_signin_first_run_infobar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "jni/AutoSigninFirstRunInfoBar_jni.h"

AutoSigninFirstRunInfoBar::AutoSigninFirstRunInfoBar(
    scoped_ptr<AutoSigninFirstRunInfoBarDelegate> delegate)
    : ConfirmInfoBar(delegate.Pass()) {}

AutoSigninFirstRunInfoBar::~AutoSigninFirstRunInfoBar() {}

base::android::ScopedJavaLocalRef<jobject>
AutoSigninFirstRunInfoBar::CreateRenderInfoBar(JNIEnv* env) {
  using base::android::ConvertUTF16ToJavaString;
  using base::android::ScopedJavaLocalRef;
  AutoSigninFirstRunInfoBarDelegate* auto_signin_infobar_delegate =
      static_cast<AutoSigninFirstRunInfoBarDelegate*>(delegate());
  ScopedJavaLocalRef<jstring> ok_button_text = ConvertUTF16ToJavaString(
      env, GetTextFor(ConfirmInfoBarDelegate::BUTTON_OK));
  ScopedJavaLocalRef<jstring> cancel_button_text = ConvertUTF16ToJavaString(
      env, GetTextFor(ConfirmInfoBarDelegate::BUTTON_CANCEL));
  ScopedJavaLocalRef<jstring> explanation_text = ConvertUTF16ToJavaString(
      env, auto_signin_infobar_delegate->GetExplanation());
  ScopedJavaLocalRef<jstring> message_text = ConvertUTF16ToJavaString(
      env, auto_signin_infobar_delegate->GetMessageText());

  return Java_AutoSigninFirstRunInfoBar_show(
      env, message_text.obj(), ok_button_text.obj(), explanation_text.obj(),
      auto_signin_infobar_delegate->message_link_range().start(),
      auto_signin_infobar_delegate->message_link_range().end());
}

void AutoSigninFirstRunInfoBar::OnLinkClicked(JNIEnv* env, jobject obj) {
  GetDelegate()->LinkClicked(NEW_FOREGROUND_TAB);
}

bool AutoSigninFirstRunInfoBar::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

scoped_ptr<infobars::InfoBar> CreateAutoSigninFirstRunInfoBar(
    scoped_ptr<AutoSigninFirstRunInfoBarDelegate> delegate) {
  return make_scoped_ptr(new AutoSigninFirstRunInfoBar(delegate.Pass()));
}
