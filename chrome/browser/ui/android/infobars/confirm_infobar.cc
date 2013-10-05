// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/confirm_infobar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/infobars/confirm_infobar_delegate.h"
#include "jni/ConfirmInfoBarDelegate_jni.h"


// ConfirmInfoBarDelegate -----------------------------------------------------

// static
InfoBar* ConfirmInfoBarDelegate::CreateInfoBar(InfoBarService* owner) {
  return new ConfirmInfoBar(owner, this);
}


// ConfirmInfoBar -------------------------------------------------------------

ConfirmInfoBar::ConfirmInfoBar(InfoBarService* owner, InfoBarDelegate* delegate)
    : InfoBarAndroid(owner, delegate),
      delegate_(delegate->AsConfirmInfoBarDelegate()),
      java_confirm_delegate_() {
}

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
  base::android::ScopedJavaLocalRef<jstring> message_text =
      base::android::ConvertUTF16ToJavaString(
          env, delegate_->GetMessageText());
  base::android::ScopedJavaLocalRef<jstring> link_text =
      base::android::ConvertUTF16ToJavaString(
          env, delegate_->GetLinkText());

  return Java_ConfirmInfoBarDelegate_showConfirmInfoBar(
      env, java_confirm_delegate_.obj(), reinterpret_cast<jint>(this),
      GetEnumeratedIconId(), message_text.obj(), link_text.obj(),
      ok_button_text.obj(), cancel_button_text.obj());
}

void ConfirmInfoBar::OnLinkClicked(JNIEnv* env, jobject obj) {
  DCHECK(delegate_);

  if (delegate_->LinkClicked(NEW_FOREGROUND_TAB))
    RemoveSelf();
}

void ConfirmInfoBar::ProcessButton(int action,
                                   const std::string& action_value) {
  DCHECK((action == InfoBarAndroid::ACTION_OK) ||
      (action == InfoBarAndroid::ACTION_CANCEL));
  if ((action == InfoBarAndroid::ACTION_OK) ?
      delegate_->Accept() : delegate_->Cancel())
    // TODO(miguelg): Consider RemoveSelf(); instead.
    CloseInfoBar();
}

string16 ConfirmInfoBar::GetTextFor(
    ConfirmInfoBarDelegate::InfoBarButton button) {
  return (delegate_->GetButtons() & button) ?
      delegate_->GetButtonLabel(button) : string16();
}


// Native JNI methods ---------------------------------------------------------

bool RegisterConfirmInfoBarDelegate(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
