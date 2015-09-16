// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/banners/app_banner_manager_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/android/banners/app_banner_data_fetcher_android.h"
#include "chrome/browser/android/shortcut_helper.h"
#include "chrome/browser/banners/app_banner_metrics.h"
#include "chrome/common/chrome_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/frame_navigate_params.h"
#include "jni/AppBannerManager_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;

namespace {

const char kPlayPlatform[] = "play";
const char kReferrerName[] = "referrer";
const char kIdName[] = "id";
const char kPlayInlineReferrer[] = "playinline=chrome_inline";

}  // anonymous namespace

namespace banners {

AppBannerManagerAndroid::AppBannerManagerAndroid(
    JNIEnv* env,
    jobject obj)
    : AppBannerManager(),
      weak_java_banner_view_manager_(env, obj) {
}

AppBannerManagerAndroid::~AppBannerManagerAndroid() {
}

void AppBannerManagerAndroid::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

void AppBannerManagerAndroid::ReplaceWebContents(JNIEnv* env,
                                                 jobject obj,
                                                 jobject jweb_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  AppBannerManager::ReplaceWebContents(web_contents);
}

bool AppBannerManagerAndroid::HandleNonWebApp(const std::string& platform,
                                              const GURL& url,
                                              const std::string& id) {
  if (!CheckPlatformAndId(platform, id))
    return false;

  banners::TrackDisplayEvent(DISPLAY_EVENT_NATIVE_APP_BANNER_REQUESTED);

  // Send the info to the Java side to get info about the app.
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jobj = weak_java_banner_view_manager_.get(env);
  if (jobj.is_null())
    return false;

  std::string id_from_app_url = ExtractQueryValueForName(url, kIdName);
  if (id_from_app_url.size() && id != id_from_app_url) {
    banners::OutputDeveloperDebugMessage(web_contents(),
                                         banners::kIgnoredIdsDoNotMatch);
    return false;
  }

  std::string referrer =
      ExtractQueryValueForName(url, kReferrerName);

  // Attach the chrome_inline referrer value, prefixed with "&" if the referrer
  // is non empty.
  if (referrer.empty())
    referrer = kPlayInlineReferrer;
  else
    referrer.append("&").append(kPlayInlineReferrer);

  ScopedJavaLocalRef<jstring> jurl(
      ConvertUTF8ToJavaString(env, data_fetcher()->validated_url().spec()));
  ScopedJavaLocalRef<jstring> jpackage(
      ConvertUTF8ToJavaString(env, id));
  ScopedJavaLocalRef<jstring> jreferrer(
      ConvertUTF8ToJavaString(env, referrer));
  Java_AppBannerManager_fetchAppDetails(
      env, jobj.obj(), jurl.obj(),
      jpackage.obj(), jreferrer.obj(),
      ShortcutHelper::GetIdealHomescreenIconSizeInDp());
  return true;
}

bool AppBannerManagerAndroid::CheckPlatformAndId(const std::string& platform,
                                                 const std::string& id) {
  if (platform != kPlayPlatform) {
    banners::OutputDeveloperDebugMessage(
        web_contents(), platform + banners::kIgnoredNotSupportedOnAndroid);
    return false;
  }
  if (id.empty()) {
    banners::OutputDeveloperDebugMessage(web_contents(), banners::kIgnoredNoId);
    return false;
  }
  return true;
}

bool AppBannerManagerAndroid::CheckFetcherMatchesContents() {
  if (!web_contents())
    return false;

  if (!data_fetcher() ||
      data_fetcher()->validated_url() != web_contents()->GetURL()) {
    banners::OutputDeveloperNotShownMessage(
        web_contents(), banners::kUserNavigatedBeforeBannerShown);
    return false;
  }
  return true;
}

std::string AppBannerManagerAndroid::ExtractQueryValueForName(
    const GURL& url,
    const std::string& name) {
  url::Component query = url.parsed_for_possibly_invalid_spec().query;
  url::Component key, value;
  const char* url_spec = url.spec().c_str();

  while (url::ExtractQueryKeyValue(url_spec, &query, &key, &value)) {
    std::string key_str(url_spec, key.begin, key.len);
    std::string value_str(url_spec, value.begin, value.len);
    if (key_str == name)
      return value_str;
  }
  return "";
}

AppBannerDataFetcher* AppBannerManagerAndroid::CreateAppBannerDataFetcher(
    base::WeakPtr<Delegate> weak_delegate) {
  return new AppBannerDataFetcherAndroid(
      web_contents(),
      weak_delegate,
      ShortcutHelper::GetIdealHomescreenIconSizeInDp(),
      ShortcutHelper::GetMinimumHomescreenIconSizeInDp(),
      ShortcutHelper::GetIdealSplashImageSizeInDp(),
      ShortcutHelper::GetMinimumSplashImageSizeInDp());
}

bool AppBannerManagerAndroid::OnAppDetailsRetrieved(JNIEnv* env,
                                                    jobject obj,
                                                    jobject japp_data,
                                                    jstring japp_title,
                                                    jstring japp_package,
                                                    jstring jicon_url) {
  if (!CheckFetcherMatchesContents())
    return false;

  base::android::ScopedJavaLocalRef<jobject> native_app_data;
  native_app_data.Reset(env, japp_data);
  GURL image_url = GURL(ConvertJavaStringToUTF8(env, jicon_url));

  AppBannerDataFetcherAndroid* android_fetcher =
      static_cast<AppBannerDataFetcherAndroid*>(data_fetcher().get());
  return android_fetcher->ContinueFetching(
      ConvertJavaStringToUTF16(env, japp_title),
      ConvertJavaStringToUTF8(env, japp_package),
      native_app_data, image_url);
}

bool AppBannerManagerAndroid::IsFetcherActive(JNIEnv* env, jobject obj) {
  return AppBannerManager::IsFetcherActive();
}

// static
bool AppBannerManagerAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

jlong Init(JNIEnv* env,
           const JavaParamRef<jobject>& obj) {
  AppBannerManagerAndroid* manager = new AppBannerManagerAndroid(env, obj);
  return reinterpret_cast<intptr_t>(manager);
}

void SetTimeDeltaForTesting(JNIEnv* env,
                            const JavaParamRef<jclass>& clazz,
                            jint days) {
  AppBannerDataFetcher::SetTimeDeltaForTesting(days);
}

void DisableSecureSchemeCheckForTesting(JNIEnv* env,
                                        const JavaParamRef<jclass>& clazz) {
  AppBannerManager::DisableSecureSchemeCheckForTesting();
}

void SetEngagementWeights(JNIEnv* env,
                          const JavaParamRef<jclass>& clazz,
                          jdouble direct_engagement,
                          jdouble indirect_engagement) {
  AppBannerManager::SetEngagementWeights(direct_engagement,
                                         indirect_engagement);
}

}  // namespace banners
