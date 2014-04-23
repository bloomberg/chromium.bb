// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_AUTO_LOGIN_INFOBAR_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_AUTO_LOGIN_INFOBAR_DELEGATE_ANDROID_H_

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/ui/auto_login_infobar_delegate.h"

class AutoLoginInfoBarDelegateAndroid : public AutoLoginInfoBarDelegate {
 public:
  AutoLoginInfoBarDelegateAndroid(const Params& params, Profile* profile);
  virtual ~AutoLoginInfoBarDelegateAndroid();

  // AutoLoginInfoBarDelegate:
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual base::string16 GetMessageText() const OVERRIDE;

  // These methods are defined in downstream code.
  bool AttachAccount(JavaObjectWeakGlobalRef weak_java_translate_helper);
  void LoginSuccess(JNIEnv* env, jobject obj, jstring result);
  void LoginFailed(JNIEnv* env, jobject obj);
  void LoginDismiss(JNIEnv* env, jobject obj);

  // Register Android JNI bindings.
  static bool Register(JNIEnv* env);

 private:
  const std::string& realm() const { return params_.header.realm; }
  const std::string& account() const { return params_.header.account; }
  const std::string& args() const { return params_.header.args; }

  JavaObjectWeakGlobalRef weak_java_auto_login_delegate_;
  std::string user_;
  const Params params_;

  DISALLOW_COPY_AND_ASSIGN(AutoLoginInfoBarDelegateAndroid);
};

#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_AUTO_LOGIN_INFOBAR_DELEGATE_ANDROID_H_

