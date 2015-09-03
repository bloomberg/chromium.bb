// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/snackbars/auto_signin_snackbar_controller.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/grit/generated_resources.h"
#include "jni/AutoSigninSnackbarController_jni.h"
#include "ui/base/l10n/l10n_util.h"

void ShowAutoSigninSnackbar(TabAndroid *tab, const base::string16& username) {
  JNIEnv *env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> message = base::android::ConvertUTF16ToJavaString(
      env, l10n_util::GetStringFUTF16(IDS_MANAGE_PASSWORDS_AUTO_SIGNIN_TITLE,
                                      username));
  Java_AutoSigninSnackbarController_showSnackbar(
      env, tab->GetJavaObject().obj(), message.obj());
}

bool RegisterAutoSigninSnackbarController(JNIEnv *env) {
   return RegisterNativesImpl(env);
}
