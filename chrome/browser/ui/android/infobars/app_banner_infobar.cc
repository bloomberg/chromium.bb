// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/app_banner_infobar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/android/banners/app_banner_infobar_delegate.h"
#include "jni/AppBannerInfoBarDelegate_jni.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"

AppBannerInfoBar::AppBannerInfoBar(
    scoped_ptr<banners::AppBannerInfoBarDelegate> delegate,
    const GURL& app_url)
    : ConfirmInfoBar(delegate.Pass()),
      app_url_(app_url) {
}

AppBannerInfoBar::~AppBannerInfoBar() {
}

base::android::ScopedJavaLocalRef<jobject>
AppBannerInfoBar::CreateRenderInfoBar(JNIEnv* env) {
  java_delegate_.Reset(Java_AppBannerInfoBarDelegate_create(env));

  base::android::ScopedJavaLocalRef<jstring> ok_button_text =
      base::android::ConvertUTF16ToJavaString(
          env, GetTextFor(ConfirmInfoBarDelegate::BUTTON_OK));

  ConfirmInfoBarDelegate* delegate = GetDelegate();
  base::android::ScopedJavaLocalRef<jstring> app_title =
      base::android::ConvertUTF16ToJavaString(
          env, delegate->GetMessageText());

  base::android::ScopedJavaLocalRef<jstring> app_url =
      base::android::ConvertUTF8ToJavaString(env, app_url_.spec());

  ScopedJavaLocalRef<jobject> java_bitmap;
  if (!delegate->GetIcon().IsEmpty()) {
    java_bitmap = gfx::ConvertToJavaBitmap(delegate->GetIcon().ToSkBitmap());
  }

  return Java_AppBannerInfoBarDelegate_showInfoBar(
      env,
      java_delegate_.obj(),
      reinterpret_cast<intptr_t>(this),
      app_title.obj(),
      java_bitmap.obj(),
      ok_button_text.obj(),
      app_url.obj());
}

// Native JNI methods ---------------------------------------------------------

bool RegisterAppBannerInfoBarDelegate(JNIEnv* env) {
 return RegisterNativesImpl(env);
}
