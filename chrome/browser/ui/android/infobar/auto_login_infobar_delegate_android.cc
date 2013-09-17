// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/auto_login_infobar_delegate_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_helper.h"
#include "base/android/jni_string.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/infobars/simple_alert_infobar_delegate.h"
#include "chrome/browser/ui/auto_login_infobar_delegate.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "jni/AutoLoginDelegate_jni.h"
#include "ui/base/l10n/l10n_util.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

AutoLoginInfoBarDelegateAndroid::AutoLoginInfoBarDelegateAndroid(
    InfoBarService* owner,
    const Params& params)
    : AutoLoginInfoBarDelegate(owner, params), params_(params) {}

AutoLoginInfoBarDelegateAndroid::~AutoLoginInfoBarDelegateAndroid() {}

bool AutoLoginInfoBarDelegateAndroid::AttachAccount(
    JavaObjectWeakGlobalRef weak_java_auto_login_delegate) {
  weak_java_auto_login_delegate_ = weak_java_auto_login_delegate;
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jrealm = ConvertUTF8ToJavaString(env, realm());
  ScopedJavaLocalRef<jstring> jaccount =
      ConvertUTF8ToJavaString(env, account());
  ScopedJavaLocalRef<jstring> jargs = ConvertUTF8ToJavaString(env, args());
  DCHECK(!jrealm.is_null() && !jaccount.is_null() && !jargs.is_null());

  ScopedJavaLocalRef<jobject> delegate =
      weak_java_auto_login_delegate_.get(env);
  DCHECK(delegate.obj());
  user_ = ConvertJavaStringToUTF8(
      Java_AutoLoginDelegate_initializeAccount(env,
                                               delegate.obj(),
                                               reinterpret_cast<jint>(this),
                                               jrealm.obj(),
                                               jaccount.obj(),
                                               jargs.obj()));
  return !user_.empty();
}

string16 AutoLoginInfoBarDelegateAndroid::GetMessageText() const {
  return l10n_util::GetStringFUTF16(IDS_AUTOLOGIN_INFOBAR_MESSAGE,
                                    UTF8ToUTF16(user_));
}

bool AutoLoginInfoBarDelegateAndroid::Accept() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> delegate =
      weak_java_auto_login_delegate_.get(env);
  DCHECK(delegate.obj());

  Java_AutoLoginDelegate_logIn(
      env, delegate.obj(), reinterpret_cast<jint>(this));

  // Do not close the infobar on accept, it will be closed as part
  // of the log in callback.
  return false;
}

bool AutoLoginInfoBarDelegateAndroid::Cancel() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> delegate =
      weak_java_auto_login_delegate_.get(env);
  DCHECK(delegate.obj());
  Java_AutoLoginDelegate_cancelLogIn(
      env, delegate.obj(), reinterpret_cast<jint>(this));
  return true;
}

void AutoLoginInfoBarDelegateAndroid::LoginSuccess(JNIEnv* env,
                                                   jobject obj,
                                                   jstring result) {
  if (owner()) {
    content::WebContents* web_contents = owner()->web_contents();
    if (web_contents) {
      web_contents->Stop();
      web_contents->OpenURL(content::OpenURLParams(
          GURL(base::android::ConvertJavaStringToUTF8(env, result)),
          content::Referrer(),
          CURRENT_TAB,
          content::PAGE_TRANSITION_AUTO_BOOKMARK,
          false));
    }
    owner()->RemoveInfoBar(this);
  }
}

void AutoLoginInfoBarDelegateAndroid::LoginFailed(JNIEnv* env, jobject obj) {
  ScopedJavaLocalRef<jobject> delegate =
      weak_java_auto_login_delegate_.get(env);
  DCHECK(delegate.obj());
  if (owner()) {
    SimpleAlertInfoBarDelegate::Create(
        owner(),
        IDR_INFOBAR_WARNING,
        l10n_util::GetStringUTF16(IDS_AUTO_LOGIN_FAILED),
        false);
    owner()->RemoveInfoBar(this);
  }
}

void AutoLoginInfoBarDelegateAndroid::LoginDismiss(JNIEnv* env, jobject obj) {
  if (owner())
    owner()->RemoveInfoBar(this);
}

// Register Android JNI bindings.
bool AutoLoginInfoBarDelegateAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
