// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/update_password_infobar.h"

#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/browser/password_manager/update_password_infobar_delegate_android.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "jni/UpdatePasswordInfoBar_jni.h"

UpdatePasswordInfoBar::UpdatePasswordInfoBar(
    std::unique_ptr<UpdatePasswordInfoBarDelegate> delegate)
    : ConfirmInfoBar(std::move(delegate)) {}

UpdatePasswordInfoBar::~UpdatePasswordInfoBar() {}

bool UpdatePasswordInfoBar::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

int UpdatePasswordInfoBar::GetIdOfSelectedUsername() const {
  return Java_UpdatePasswordInfoBar_getSelectedUsername(
      base::android::AttachCurrentThread(), java_infobar_.obj());
}

base::android::ScopedJavaLocalRef<jobject>
UpdatePasswordInfoBar::CreateRenderInfoBar(JNIEnv* env) {
  using base::android::ConvertUTF16ToJavaString;
  using base::android::ScopedJavaLocalRef;
  UpdatePasswordInfoBarDelegate* update_password_delegate =
      static_cast<UpdatePasswordInfoBarDelegate*>(delegate());
  ScopedJavaLocalRef<jstring> ok_button_text = ConvertUTF16ToJavaString(
      env, GetTextFor(ConfirmInfoBarDelegate::BUTTON_OK));
  ScopedJavaLocalRef<jstring> cancel_button_text = ConvertUTF16ToJavaString(
      env, GetTextFor(ConfirmInfoBarDelegate::BUTTON_CANCEL));
  ScopedJavaLocalRef<jstring> branding_text =
      ConvertUTF16ToJavaString(env, update_password_delegate->GetBranding());

  std::vector<base::string16> usernames;
  if (update_password_delegate->ShowMultipleAccounts()) {
    for (auto password_form : update_password_delegate->GetCurrentForms())
      usernames.push_back(password_form->username_value);
  } else {
    usernames.push_back(
        update_password_delegate->get_username_for_single_account());
  }

  base::android::ScopedJavaLocalRef<jobject> infobar;
  infobar.Reset(Java_UpdatePasswordInfoBar_show(
      env, reinterpret_cast<intptr_t>(this), GetEnumeratedIconId(),
      base::android::ToJavaArrayOfStrings(env, usernames).obj(),
      ok_button_text.obj(), cancel_button_text.obj(), branding_text.obj(),
      update_password_delegate->ShowMultipleAccounts(),
      update_password_delegate->is_smartlock_branding_enabled()));

  java_infobar_.Reset(env, infobar.obj());
  return infobar;
}

void UpdatePasswordInfoBar::OnLinkClicked(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj) {
  GetDelegate()->LinkClicked(NEW_FOREGROUND_TAB);
}
