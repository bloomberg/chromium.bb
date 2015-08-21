// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/snackbars/auto_signin_snackbar_controller.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/android/tab_android.h"
#include "jni/AutoSigninSnackbarController_jni.h"

void ShowAutoSigninSnackbar(TabAndroid *tab, const base::string16& username) {
  JNIEnv *env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> message =
      base::android::ConvertUTF16ToJavaString(
          env, username);
  Java_AutoSigninSnackbarController_showSnackbar(
      env, tab->GetJavaObject().obj(), message.obj());
}

bool RegisterAutoSigninSnackbarController(JNIEnv *env) {
   return RegisterNativesImpl(env);
}
