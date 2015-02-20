// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/account_chooser_infobar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/password_manager/account_chooser_infobar_delegate_android.h"
#include "components/password_manager/content/common/credential_manager_types.h"
#include "jni/AccountChooserInfoBar_jni.h"

AccountChooserInfoBar::AccountChooserInfoBar(
    scoped_ptr<AccountChooserInfoBarDelegateAndroid> delegate)
    : InfoBarAndroid(delegate.Pass()) {
}

AccountChooserInfoBar::~AccountChooserInfoBar() {
}

base::android::ScopedJavaLocalRef<jobject>
AccountChooserInfoBar::CreateRenderInfoBar(JNIEnv* env) {
  std::vector<base::string16> usernames;
  // TODO(melandory): Federated credentials should be processed also.
  for (auto password_form : GetDelegate()->local_credentials_forms())
    usernames.push_back(password_form->username_value);
  base::android::ScopedJavaLocalRef<jobjectArray> java_usernames =
      base::android::ToJavaArrayOfStrings(env, usernames);
  return Java_AccountChooserInfoBar_show(env, reinterpret_cast<intptr_t>(this),
                                         GetEnumeratedIconId(),
                                         java_usernames.obj());
}

void AccountChooserInfoBar::OnCredentialClicked(JNIEnv* env,
                                                jobject obj,
                                                jint credential_item,
                                                jint credential_type) {
  GetDelegate()->ChooseCredential(
      credential_item,
      static_cast<password_manager::CredentialType>(credential_type));
  RemoveSelf();
}

void AccountChooserInfoBar::ProcessButton(int action,
                                          const std::string& action_value) {
  if (!owner())
    return;  // We're closing; don't call anything, it might access the owner.
  GetDelegate()->ChooseCredential(
      -1, password_manager::CredentialType::CREDENTIAL_TYPE_EMPTY);
  RemoveSelf();
}

AccountChooserInfoBarDelegateAndroid* AccountChooserInfoBar::GetDelegate() {
  return static_cast<AccountChooserInfoBarDelegateAndroid*>(delegate());
}

bool RegisterAccountChooserInfoBar(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
