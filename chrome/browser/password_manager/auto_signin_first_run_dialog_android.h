// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_AUTO_SIGNIN_FIRST_RUN_DIALOG_ANDROID_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_AUTO_SIGNIN_FIRST_RUN_DIALOG_ANDROID_H_

#include <vector>

#include "base/android/jni_android.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/ui/passwords/manage_passwords_state.h"

namespace content {
class WebContents;
}

// Native counterpart for the android dialog which informs user about auto
// sign-in feature and allows to turn it off.
class AutoSigninFirstRunDialogAndroid {
 public:
  explicit AutoSigninFirstRunDialogAndroid(content::WebContents* web_contents);

  ~AutoSigninFirstRunDialogAndroid();
  void Destroy(JNIEnv* env, jobject obj);

  void ShowDialog();

  // Closes the dialog and propagates that no credentials was chosen.
  void CancelDialog(JNIEnv* env, jobject obj);

  // Opens new tab with page which explains the Smart Lock branding.
  void OnLinkClicked(JNIEnv* env, jobject obj);

  // Records the user decision to use auto sign-in feature.
  void OnOkClicked(JNIEnv* env, jobject obj);

  // Opts user out of the auto sign-in feature.
  void OnTurnOffClicked(JNIEnv* env, jobject obj);

 private:
  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(AutoSigninFirstRunDialogAndroid);
};

// Native JNI methods
bool RegisterAutoSigninFirstRunDialogAndroid(JNIEnv* env);

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_AUTO_SIGNIN_FIRST_RUN_DIALOG_ANDROID_H_
