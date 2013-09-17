// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobar/confirm_infobar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/infobars/confirm_infobar_delegate.h"
#include "jni/ConfirmInfoBarDelegate_jni.h"

using base::android::ConvertUTF16ToJavaString;
using base::android::ScopedJavaLocalRef;

// static
InfoBar* ConfirmInfoBarDelegate::CreateInfoBar(InfoBarService* owner) {
  return new ConfirmInfoBar(owner, this);
}

ConfirmInfoBar::ConfirmInfoBar(InfoBarService* owner, InfoBarDelegate* delegate)
    : InfoBarAndroid(owner, delegate),
      delegate_(delegate->AsConfirmInfoBarDelegate()),
      java_confirm_delegate_() {}

ConfirmInfoBar::~ConfirmInfoBar() {}

ScopedJavaLocalRef<jobject> ConfirmInfoBar::CreateRenderInfoBar(JNIEnv* env) {
  java_confirm_delegate_.Reset(Java_ConfirmInfoBarDelegate_create(env));
  ScopedJavaLocalRef<jstring> ok_button_text =
      ConvertUTF16ToJavaString(env, GetTextFor(InfoBarAndroid::ACTION_OK));
  ScopedJavaLocalRef<jstring> cancel_button_text =
      ConvertUTF16ToJavaString(env, GetTextFor(InfoBarAndroid::ACTION_CANCEL));
  ScopedJavaLocalRef<jstring> message_text =
      ConvertUTF16ToJavaString(env, GetMessage());

  return Java_ConfirmInfoBarDelegate_showConfirmInfoBar(
      env,
      java_confirm_delegate_.obj(),
      reinterpret_cast<jint>(this),
      GetEnumeratedIconId(),
      message_text.obj(),
      ok_button_text.obj(),
      cancel_button_text.obj());
}

void ConfirmInfoBar::ProcessButton(int action,
                                   const std::string& action_value) {
  DCHECK(action == InfoBarAndroid::ACTION_OK ||
         action == InfoBarAndroid::ACTION_CANCEL);
  if ((action == InfoBarAndroid::ACTION_OK) ? delegate_->Accept()
                                            : delegate_->Cancel())
    CloseInfoBar();
}

string16 ConfirmInfoBar::GetTextFor(ActionType action) {
  int buttons = delegate_->GetButtons();
  switch (action) {
    case InfoBarAndroid::ACTION_OK:
      if (buttons & ConfirmInfoBarDelegate::BUTTON_OK)
        return delegate_->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK);
      break;
    case InfoBarAndroid::ACTION_CANCEL:
      if (buttons & ConfirmInfoBarDelegate::BUTTON_CANCEL)
        return delegate_->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_CANCEL);
      break;
    default:
      break;
  }
  return string16();
}

string16 ConfirmInfoBar::GetMessage() {
  return delegate_->GetMessageText();
}

// -----------------------------------------------------------------------------
// Native JNI methods for confirm delegate.
// -----------------------------------------------------------------------------
bool RegisterConfirmInfoBarDelegate(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
