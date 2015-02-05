// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/app_banner_infobar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/android/banners/app_banner_infobar_delegate.h"
#include "jni/AppBannerInfoBar_jni.h"
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
  ConfirmInfoBarDelegate* app_banner_infobar_delegate = GetDelegate();

  base::android::ScopedJavaLocalRef<jstring> app_title =
      base::android::ConvertUTF16ToJavaString(
          env, app_banner_infobar_delegate->GetMessageText());

  base::android::ScopedJavaLocalRef<jobject> java_bitmap;
  if (!app_banner_infobar_delegate->GetIcon().IsEmpty()) {
    java_bitmap = gfx::ConvertToJavaBitmap(
        app_banner_infobar_delegate->GetIcon().ToSkBitmap());
  }

  base::android::ScopedJavaLocalRef<jobject> infobar;
  base::android::ScopedJavaLocalRef<jstring> app_url =
      base::android::ConvertUTF8ToJavaString(env, app_url_.spec());

  infobar.Reset(Java_AppBannerInfoBar_createWebAppInfoBar(
      env,
      reinterpret_cast<intptr_t>(this),
      app_title.obj(),
      java_bitmap.obj(),
      app_url.obj()));

  java_infobar_.Reset(env, infobar.obj());
  return infobar;
}

// Native JNI methods ---------------------------------------------------------

bool RegisterAppBannerInfoBarDelegate(JNIEnv* env) {
 return RegisterNativesImpl(env);
}
