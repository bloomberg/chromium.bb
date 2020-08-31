// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/update_password_infobar.h"

#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/UpdatePasswordInfoBar_jni.h"
#include "chrome/browser/password_manager/android/update_password_infobar_delegate_android.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "ui/base/l10n/l10n_util.h"

using base::android::JavaParamRef;

UpdatePasswordInfoBar::UpdatePasswordInfoBar(
    std::unique_ptr<UpdatePasswordInfoBarDelegate> delegate)
    : ConfirmInfoBar(std::move(delegate)) {}

UpdatePasswordInfoBar::~UpdatePasswordInfoBar() {}

int UpdatePasswordInfoBar::GetIdOfSelectedUsername() const {
  return Java_UpdatePasswordInfoBar_getSelectedUsername(
      base::android::AttachCurrentThread(), java_infobar_);
}

base::android::ScopedJavaLocalRef<jobject>
UpdatePasswordInfoBar::CreateRenderInfoBar(JNIEnv* env) {
  using base::android::ConvertUTF16ToJavaString;
  using base::android::ScopedJavaLocalRef;
  UpdatePasswordInfoBarDelegate* update_password_delegate =
      static_cast<UpdatePasswordInfoBarDelegate*>(delegate());
  ScopedJavaLocalRef<jstring> ok_button_text = ConvertUTF16ToJavaString(
      env, GetTextFor(ConfirmInfoBarDelegate::BUTTON_OK));
  ScopedJavaLocalRef<jstring> message_text =
      ConvertUTF16ToJavaString(env, update_password_delegate->GetMessageText());
  ScopedJavaLocalRef<jstring> details_message_text = ConvertUTF16ToJavaString(
      env, update_password_delegate->GetDetailsMessageText());

  std::vector<base::string16> usernames;
  unsigned int selected_username =
      update_password_delegate->GetDisplayUsernames(&usernames);
  ScopedJavaLocalRef<jobjectArray> display_usernames =
      base::android::ToJavaArrayOfStrings(env, usernames);

  base::android::ScopedJavaLocalRef<jobject> infobar;
  infobar.Reset(Java_UpdatePasswordInfoBar_show(
      env, GetJavaIconId(), display_usernames, selected_username, message_text,
      details_message_text, ok_button_text));

  java_infobar_.Reset(env, infobar.obj());
  return infobar;
}

void UpdatePasswordInfoBar::OnLinkClicked(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj) {
  delegate()->LinkClicked(WindowOpenDisposition::NEW_FOREGROUND_TAB);
}
