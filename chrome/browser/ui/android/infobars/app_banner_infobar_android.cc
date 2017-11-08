// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/app_banner_infobar_android.h"

#include <memory>
#include <string>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/banners/app_banner_infobar_delegate_android.h"
#include "jni/AppBannerInfoBarAndroid_jni.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ui/gfx/android/java_bitmap.h"

AppBannerInfoBarAndroid::AppBannerInfoBarAndroid(
    std::unique_ptr<banners::AppBannerInfoBarDelegateAndroid> delegate,
    const base::android::ScopedJavaGlobalRef<jobject>& japp_data)
    : ConfirmInfoBar(std::move(delegate)), japp_data_(japp_data) {}

AppBannerInfoBarAndroid::AppBannerInfoBarAndroid(
    std::unique_ptr<banners::AppBannerInfoBarDelegateAndroid> delegate,
    const GURL& app_url)
    : ConfirmInfoBar(std::move(delegate)), app_url_(app_url) {}

AppBannerInfoBarAndroid::~AppBannerInfoBarAndroid() {}

base::android::ScopedJavaLocalRef<jobject>
AppBannerInfoBarAndroid::CreateRenderInfoBar(JNIEnv* env) {
  banners::AppBannerInfoBarDelegateAndroid* delegate = GetDelegate();

  base::android::ScopedJavaLocalRef<jstring> app_title =
      base::android::ConvertUTF16ToJavaString(env, delegate->GetMessageText());

  DCHECK(!delegate->GetPrimaryIcon().drawsNothing());
  base::android::ScopedJavaLocalRef<jobject> java_bitmap =
      gfx::ConvertToJavaBitmap(&delegate->GetPrimaryIcon());

  base::android::ScopedJavaLocalRef<jobject> infobar;
  if (!japp_data_.is_null()) {
    infobar.Reset(Java_AppBannerInfoBarAndroid_createNativeAppInfoBar(
        env, app_title, java_bitmap, japp_data_));
  } else {
    // Trim down the app URL to the domain and registry.
    std::string trimmed_url =
        net::registry_controlled_domains::GetDomainAndRegistry(
            app_url_,
            net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

    base::android::ScopedJavaLocalRef<jstring> app_url =
        base::android::ConvertUTF8ToJavaString(env, trimmed_url);

    infobar.Reset(Java_AppBannerInfoBarAndroid_createWebAppInfoBar(
        env, app_title, java_bitmap, app_url));
  }

  java_infobar_.Reset(env, infobar.obj());
  return infobar;
}

void AppBannerInfoBarAndroid::OnInstallStateChanged(int new_state) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AppBannerInfoBarAndroid_onInstallStateChanged(env, java_infobar_,
                                                     new_state);
}

banners::AppBannerInfoBarDelegateAndroid*
AppBannerInfoBarAndroid::GetDelegate() {
  return static_cast<banners::AppBannerInfoBarDelegateAndroid*>(delegate());
}
