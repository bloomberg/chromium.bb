// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/credential_android.h"

#include "base/android/jni_string.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "jni/Credential_jni.h"
#include "ui/base/l10n/l10n_util.h"

base::android::ScopedJavaLocalRef<jobject> CreateNativeCredential(
    JNIEnv* env,
    const autofill::PasswordForm& password_form,
    int position,
    int type) {
  using base::android::ConvertUTF16ToJavaString;
  using base::android::ConvertUTF8ToJavaString;
  std::string federation =
      password_form.federation_url.is_empty()
          ? std::string()
          : l10n_util::GetStringFUTF8(
                IDS_PASSWORDS_VIA_FEDERATION,
                base::ASCIIToUTF16(password_form.federation_url.host()));
  return Java_Credential_createCredential(
      env, ConvertUTF16ToJavaString(env, password_form.username_value).obj(),
      ConvertUTF16ToJavaString(env, password_form.display_name).obj(),
      ConvertUTF8ToJavaString(env, federation).obj(), type, position);
}

base::android::ScopedJavaLocalRef<jobjectArray> CreateNativeCredentialArray(
    JNIEnv* env,
    size_t size) {
  return Java_Credential_createCredentialArray(env, static_cast<int>(size));
}

bool RegisterCredential(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
