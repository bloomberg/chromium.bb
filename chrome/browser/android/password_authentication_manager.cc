// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/password_authentication_manager.h"

#include "chrome/browser/android/tab_android.h"
#include "jni/PasswordAuthenticationManager_jni.h"

namespace {

class PasswordAuthenticationCallback {
 public:
  explicit PasswordAuthenticationCallback(
      const base::Closure& success_callback) {
    success_callback_ = success_callback;
  }

  void OnResult(bool result) {
    if (result)
      success_callback_.Run();
    delete this;
  }

 private:
  virtual ~PasswordAuthenticationCallback() {}

  base::Closure success_callback_;
};

}  // namespace

PasswordAuthenticationManager::PasswordAuthenticationManager() {
}

PasswordAuthenticationManager::~PasswordAuthenticationManager() {
}

bool PasswordAuthenticationManager::RegisterPasswordAuthenticationManager(
    JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void PasswordAuthenticationManager::AuthenticatePasswordAutofill(
    content::WebContents* web_contents,
    const base::Closure& success_callback) {
  TabAndroid* tab = TabAndroid::FromWebContents(web_contents);
  if (!tab)
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  PasswordAuthenticationCallback* auth_callback =
      new PasswordAuthenticationCallback(success_callback);
  Java_PasswordAuthenticationManager_requestAuthentication(
      env,
      tab->GetJavaObject().obj(),
      Java_PasswordAuthenticationCallback_create(
          env,
          reinterpret_cast<intptr_t>(auth_callback)).obj());
}

// static
void OnResult(JNIEnv* env,
              jclass jcaller,
              jlong callback_ptr,
              jboolean authenticated) {
  PasswordAuthenticationCallback* callback =
      reinterpret_cast<PasswordAuthenticationCallback*>(callback_ptr);
  callback->OnResult(authenticated);
}
