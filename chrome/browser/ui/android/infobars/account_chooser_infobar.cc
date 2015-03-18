// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/account_chooser_infobar.h"

#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/password_manager/account_chooser_infobar_delegate_android.h"
#include "chrome/browser/password_manager/credential_android.h"
#include "components/password_manager/content/common/credential_manager_types.h"
#include "jni/AccountChooserInfoBar_jni.h"

namespace {

void AddElementsToJavaCredentialArray(
    JNIEnv* env,
    ScopedJavaLocalRef<jobjectArray> java_credentials_array,
    const std::vector<const autofill::PasswordForm*>& password_forms,
    password_manager::CredentialType type,
    int indexStart = 0) {
  int index = indexStart;
  for (auto password_form : password_forms) {
    ScopedJavaLocalRef<jobject> java_credential = CreateNativeCredential(
        env, *password_form, index - indexStart, static_cast<int>(type));
    env->SetObjectArrayElement(java_credentials_array.obj(), index,
                               java_credential.obj());
    index++;
  }
}

};  // namespace

AccountChooserInfoBar::AccountChooserInfoBar(
    scoped_ptr<AccountChooserInfoBarDelegateAndroid> delegate)
    : InfoBarAndroid(delegate.Pass()) {
}

AccountChooserInfoBar::~AccountChooserInfoBar() {
}

base::android::ScopedJavaLocalRef<jobject>
AccountChooserInfoBar::CreateRenderInfoBar(JNIEnv* env) {
  size_t credential_array_size =
      GetDelegate()->local_credentials_forms().size() +
      GetDelegate()->federated_credentials_forms().size();
  ScopedJavaLocalRef<jobjectArray> java_credentials_array =
      CreateNativeCredentialArray(env, credential_array_size);
  AddElementsToJavaCredentialArray(
      env, java_credentials_array, GetDelegate()->local_credentials_forms(),
      password_manager::CredentialType::CREDENTIAL_TYPE_LOCAL);
  AddElementsToJavaCredentialArray(
      env, java_credentials_array, GetDelegate()->federated_credentials_forms(),
      password_manager::CredentialType::CREDENTIAL_TYPE_FEDERATED,
      GetDelegate()->local_credentials_forms().size());
  return Java_AccountChooserInfoBar_show(env, reinterpret_cast<intptr_t>(this),
                                         GetEnumeratedIconId(),
                                         java_credentials_array.obj());
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
