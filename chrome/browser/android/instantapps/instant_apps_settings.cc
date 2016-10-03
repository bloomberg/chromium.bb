// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/instantapps/instant_apps_settings.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/installable/installable_logging.h"
#include "content/public/browser/web_contents.h"
#include "jni/InstantAppsSettings_jni.h"
#include "url/gurl.h"

using base::android::JavaParamRef;
using base::android::ConvertJavaStringToUTF8;

void InstantAppsSettings::RecordInfoBarShowEvent(
    content::WebContents* web_contents,
    const std::string& url) {
  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents,
      GURL(url),
      AppBannerSettingsHelper::kInstantAppsKey,
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_SHOW,
      base::Time::Now());
}

void InstantAppsSettings::RecordInfoBarDismissEvent(
    content::WebContents* web_contents,
    const std::string& url) {
  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents,
      GURL(url),
      AppBannerSettingsHelper::kInstantAppsKey,
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_BLOCK,
      base::Time::Now());
}

static void SetInstantAppDefault(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jobject>& jweb_contents,
    const JavaParamRef<jstring>& jurl) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);

  std::string url(ConvertJavaStringToUTF8(env, jurl));

  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents,
      GURL(url),
      AppBannerSettingsHelper::kInstantAppsKey,
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_ADD_TO_HOMESCREEN,
      base::Time::Now());
}

static jboolean GetInstantAppDefault(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jobject>& jweb_contents,
    const JavaParamRef<jstring>& jurl) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);

  std::string url(ConvertJavaStringToUTF8(env, jurl));

  base::Time added_time = AppBannerSettingsHelper::GetSingleBannerEvent(
      web_contents,
      GURL(url),
      AppBannerSettingsHelper::kInstantAppsKey,
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_ADD_TO_HOMESCREEN);

  return !added_time.is_null();
}

static jboolean ShouldShowBanner(JNIEnv* env,
                                 const JavaParamRef<jclass>& clazz,
                                 const JavaParamRef<jobject>& jweb_contents,
                                 const JavaParamRef<jstring>& jurl) {
  content::WebContents* web_contents =
        content::WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);

  std::string url(ConvertJavaStringToUTF8(env, jurl));

  return AppBannerSettingsHelper::ShouldShowBanner(
      web_contents,
      GURL(url),
      AppBannerSettingsHelper::kInstantAppsKey,
      base::Time::Now()) == InstallableStatusCode::NO_ERROR_DETECTED;
}

bool RegisterInstantAppsSettings(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
