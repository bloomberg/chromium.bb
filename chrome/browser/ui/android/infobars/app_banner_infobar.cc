// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/app_banner_infobar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/android/banners/app_banner_infobar_delegate.h"
#include "jni/AppBannerInfoBar_jni.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"


AppBannerInfoBar::AppBannerInfoBar(
    scoped_ptr<banners::AppBannerInfoBarDelegate> delegate,
    const base::android::ScopedJavaGlobalRef<jobject>& japp_data)
    : ConfirmInfoBar(delegate.Pass()),
      japp_data_(japp_data) {
}

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
  if (!japp_data_.is_null()) {
    infobar.Reset(Java_AppBannerInfoBar_createNativeAppInfoBar(
        env,
        reinterpret_cast<intptr_t>(this),
        app_title.obj(),
        java_bitmap.obj(),
        japp_data_.obj()));
  } else {
    // Trim down the app URL to the domain and registry.
    std::string trimmed_url =
        net::registry_controlled_domains::GetDomainAndRegistry(
            app_url_,
            net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

    base::android::ScopedJavaLocalRef<jstring> app_url =
        base::android::ConvertUTF8ToJavaString(env, trimmed_url);

    infobar.Reset(Java_AppBannerInfoBar_createWebAppInfoBar(
        env,
        reinterpret_cast<intptr_t>(this),
        app_title.obj(),
        java_bitmap.obj(),
        app_url.obj()));
  }

  java_infobar_.Reset(env, infobar.obj());
  return infobar;
}

void AppBannerInfoBar::OnInstallStateChanged(int new_state) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AppBannerInfoBar_onInstallStateChanged(env,
                                              java_infobar_.obj(),
                                              new_state);
}

// Native JNI methods ---------------------------------------------------------

bool RegisterAppBannerInfoBar(JNIEnv* env) {
 return RegisterNativesImpl(env);
}
