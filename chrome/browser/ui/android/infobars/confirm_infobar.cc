// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/confirm_infobar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "chrome/browser/android/resource_mapper.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "jni/ConfirmInfoBarDelegate_jni.h"


// ConfirmInfoBarDelegate -----------------------------------------------------

// static
scoped_ptr<infobars::InfoBar> ConfirmInfoBarDelegate::CreateInfoBar(
    scoped_ptr<ConfirmInfoBarDelegate> delegate) {
  return scoped_ptr<infobars::InfoBar>(new ConfirmInfoBar(delegate.Pass()));
}


// ConfirmInfoBar -------------------------------------------------------------

ConfirmInfoBar::ConfirmInfoBar(scoped_ptr<ConfirmInfoBarDelegate> delegate)
    : InfoBarAndroid(delegate.PassAs<infobars::InfoBarDelegate>()),
      java_confirm_delegate_() {}

ConfirmInfoBar::~ConfirmInfoBar() {
}

base::android::ScopedJavaLocalRef<jobject> ConfirmInfoBar::CreateRenderInfoBar(
    JNIEnv* env) {
  java_confirm_delegate_.Reset(Java_ConfirmInfoBarDelegate_create(env));
  base::android::ScopedJavaLocalRef<jstring> ok_button_text =
      base::android::ConvertUTF16ToJavaString(
          env, GetTextFor(ConfirmInfoBarDelegate::BUTTON_OK));
  base::android::ScopedJavaLocalRef<jstring> cancel_button_text =
      base::android::ConvertUTF16ToJavaString(
          env, GetTextFor(ConfirmInfoBarDelegate::BUTTON_CANCEL));
  ConfirmInfoBarDelegate* delegate = GetDelegate();
  base::android::ScopedJavaLocalRef<jstring> message_text =
      base::android::ConvertUTF16ToJavaString(
          env, delegate->GetMessageText());
  base::android::ScopedJavaLocalRef<jstring> link_text =
      base::android::ConvertUTF16ToJavaString(
          env, delegate->GetLinkText());

  return Java_ConfirmInfoBarDelegate_showConfirmInfoBar(
      env, java_confirm_delegate_.obj(), reinterpret_cast<intptr_t>(this),
      GetEnumeratedIconId(), message_text.obj(), link_text.obj(),
      ok_button_text.obj(), cancel_button_text.obj());
}

void ConfirmInfoBar::OnLinkClicked(JNIEnv* env, jobject obj) {
  if (!owner())
      return; // We're closing; don't call anything, it might access the owner.

  if (GetDelegate()->LinkClicked(NEW_FOREGROUND_TAB))
    RemoveSelf();
}

void ConfirmInfoBar::ProcessButton(int action,
                                   const std::string& action_value) {
  if (!owner())
    return; // We're closing; don't call anything, it might access the owner.

  DCHECK((action == InfoBarAndroid::ACTION_OK) ||
      (action == InfoBarAndroid::ACTION_CANCEL));
  ConfirmInfoBarDelegate* delegate = GetDelegate();
  if ((action == InfoBarAndroid::ACTION_OK) ?
      delegate->Accept() : delegate->Cancel())
    RemoveSelf();
}

ConfirmInfoBarDelegate* ConfirmInfoBar::GetDelegate() {
  return delegate()->AsConfirmInfoBarDelegate();
}

base::string16 ConfirmInfoBar::GetTextFor(
    ConfirmInfoBarDelegate::InfoBarButton button) {
  ConfirmInfoBarDelegate* delegate = GetDelegate();
  return (delegate->GetButtons() & button) ?
      delegate->GetButtonLabel(button) : base::string16();
}


// Native JNI methods ---------------------------------------------------------

bool RegisterConfirmInfoBarDelegate(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
