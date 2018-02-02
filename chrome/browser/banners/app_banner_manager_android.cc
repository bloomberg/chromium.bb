// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/app_banner_manager_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/shortcut_helper.h"
#include "chrome/browser/android/webapk/chrome_webapk_host.h"
#include "chrome/browser/android/webapk/webapk_web_manifest_checker.h"
#include "chrome/browser/banners/app_banner_infobar_delegate_android.h"
#include "chrome/browser/banners/app_banner_metrics.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/installable/pwa_ambient_badge_manager_android.h"
#include "content/public/browser/manifest_icon_downloader.h"
#include "content/public/browser/web_contents.h"
#include "jni/AppBannerManager_jni.h"
#include "net/base/url_util.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(banners::AppBannerManagerAndroid);

namespace banners {

AppBannerManagerAndroid::AppBannerManagerAndroid(
    content::WebContents* web_contents)
    : AppBannerManager(web_contents),
      ambient_badge_manager_(
          std::make_unique<PwaAmbientBadgeManagerAndroid>(web_contents)) {
  can_install_webapk_ = ChromeWebApkHost::CanInstallWebApk();
  CreateJavaBannerManager();
}

AppBannerManagerAndroid::~AppBannerManagerAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AppBannerManager_destroy(env, java_banner_manager_);
  java_banner_manager_.Reset();
}

const base::android::ScopedJavaGlobalRef<jobject>&
AppBannerManagerAndroid::GetJavaBannerManager() const {
  return java_banner_manager_;
}

bool AppBannerManagerAndroid::IsRunningForTesting(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return IsRunning();
}

void AppBannerManagerAndroid::RecordMenuOpen(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj) {
  manager()->RecordMenuOpenHistogram();
}

void AppBannerManagerAndroid::RecordMenuItemAddToHomescreen(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  manager()->RecordMenuItemAddToHomescreenHistogram();
}

bool AppBannerManagerAndroid::OnAppDetailsRetrieved(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& japp_data,
    const JavaParamRef<jstring>& japp_title,
    const JavaParamRef<jstring>& japp_package,
    const JavaParamRef<jstring>& jicon_url) {
  UpdateState(State::ACTIVE);
  native_app_data_.Reset(japp_data);
  native_app_title_ = ConvertJavaStringToUTF16(env, japp_title);
  native_app_package_ = ConvertJavaStringToUTF8(env, japp_package);
  primary_icon_url_ = GURL(ConvertJavaStringToUTF8(env, jicon_url));

  if (!CheckIfShouldShowBanner())
    return false;

  return content::ManifestIconDownloader::Download(
      web_contents(), primary_icon_url_,
      ShortcutHelper::GetIdealHomescreenIconSizeInPx(),
      ShortcutHelper::GetMinimumHomescreenIconSizeInPx(),
      base::Bind(&AppBannerManager::OnAppIconFetched, GetWeakPtr()));
}

void AppBannerManagerAndroid::RequestAppBanner(const GURL& validated_url,
                                               bool is_debug_mode) {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!Java_AppBannerManager_isEnabledForTab(env, java_banner_manager_))
    return;

  AppBannerManager::RequestAppBanner(validated_url, is_debug_mode);
}

std::string AppBannerManagerAndroid::GetAppIdentifier() {
  return native_app_data_.is_null() ? AppBannerManager::GetAppIdentifier()
                                    : native_app_package_;
}

std::string AppBannerManagerAndroid::GetBannerType() {
  return native_app_data_.is_null() ? AppBannerManager::GetBannerType()
                                    : "play";
}

bool AppBannerManagerAndroid::IsWebAppConsideredInstalled(
    content::WebContents* web_contents,
    const GURL& validated_url,
    const GURL& start_url,
    const GURL& manifest_url) {
  // Whether a WebAPK is installed or is being installed. IsWebApkInstalled
  // will still detect the presence of a WebAPK even if Chrome's data is
  // cleared.
  bool is_webapk_installed = ShortcutHelper::IsWebApkInstalled(
      web_contents->GetBrowserContext(), start_url, manifest_url);

  // If a WebAPK is not installed, we use a heuristic to decide whether we
  // consider a non-WebAPK to be installed (due to the lack of a pre-Oreo API
  // to detect what is and isn't on the Android homescreen).
  // TODO(crbug.com/786268): stop relying on this heuristic once WebAPKs are
  // common vs legacy PWAs.
  return is_webapk_installed ||
         AppBannerSettingsHelper::HasBeenInstalled(web_contents, validated_url,
                                                   GetAppIdentifier());
}

InstallableParams AppBannerManagerAndroid::ParamsToPerformInstallableCheck() {
  InstallableParams params =
      AppBannerManager::ParamsToPerformInstallableCheck();
  params.valid_badge_icon = can_install_webapk_;

  return params;
}

void AppBannerManagerAndroid::PerformInstallableCheck() {
  // Check if the manifest prefers that we show a native app banner. If so, call
  // to Java to verify the details.
  if (manifest_.prefer_related_applications &&
      manifest_.related_applications.size() &&
      !java_banner_manager_.is_null()) {
    InstallableStatusCode code = NO_ERROR_DETECTED;
    for (const auto& application : manifest_.related_applications) {
      std::string platform = base::UTF16ToUTF8(application.platform.string());
      std::string id = base::UTF16ToUTF8(application.id.string());
      code = QueryNativeApp(platform, application.url, id);
      if (code == NO_ERROR_DETECTED)
        return;
    }

    // We must have some error in |code| if we reached this point, so report it.
    Stop(code);
    return;
  }

  if (can_install_webapk_ && !AreWebManifestUrlsWebApkCompatible(manifest_)) {
    Stop(URL_NOT_SUPPORTED_FOR_WEBAPK);
    return;
  }

  // No native app banner was requested. Continue checking for a web app banner.
  AppBannerManager::PerformInstallableCheck();
}

void AppBannerManagerAndroid::OnDidPerformInstallableCheck(
    const InstallableData& data) {
  if (data.badge_icon && !data.badge_icon->drawsNothing()) {
    DCHECK(!data.badge_icon_url.is_empty());

    badge_icon_url_ = data.badge_icon_url;
    badge_icon_ = *data.badge_icon;
  }

  AppBannerManager::OnDidPerformInstallableCheck(data);
}

void AppBannerManagerAndroid::OnAppIconFetched(const SkBitmap& bitmap) {
  if (bitmap.drawsNothing()) {
    Stop(NO_ICON_AVAILABLE);
    return;
  }

  primary_icon_ = bitmap;
  SendBannerPromptRequest();
}

void AppBannerManagerAndroid::ResetCurrentPageData() {
  AppBannerManager::ResetCurrentPageData();
  native_app_data_.Reset();
  native_app_package_ = "";
}

void AppBannerManagerAndroid::ShowBannerUi(WebappInstallSource install_source) {
  content::WebContents* contents = web_contents();
  DCHECK(contents);

  if (native_app_data_.is_null()) {
    if (AppBannerInfoBarDelegateAndroid::Create(
            contents, GetWeakPtr(),
            ShortcutHelper::CreateShortcutInfo(
                manifest_url_, manifest_, primary_icon_url_, badge_icon_url_),
            primary_icon_, badge_icon_, install_source, can_install_webapk_)) {
      RecordDidShowBanner("AppBanner.WebApp.Shown");
      TrackDisplayEvent(DISPLAY_EVENT_WEB_APP_BANNER_CREATED);
      ReportStatus(SHOWING_WEB_APP_BANNER);
    } else {
      ReportStatus(FAILED_TO_CREATE_BANNER);
    }
  } else {
    if (AppBannerInfoBarDelegateAndroid::Create(
            contents, native_app_title_, native_app_data_, primary_icon_,
            native_app_package_, referrer_)) {
      RecordDidShowBanner("AppBanner.NativeApp.Shown");
      TrackDisplayEvent(DISPLAY_EVENT_NATIVE_APP_BANNER_CREATED);
      ReportStatus(SHOWING_NATIVE_APP_BANNER);
    } else {
      ReportStatus(FAILED_TO_CREATE_BANNER);
    }
  }
}

void AppBannerManagerAndroid::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (IsExperimentalAppBannersEnabled())
    ambient_badge_manager_->MaybeShowBadge();
  AppBannerManager::DidFinishLoad(render_frame_host, validated_url);
}

InstallableStatusCode AppBannerManagerAndroid::QueryNativeApp(
    const std::string& platform,
    const GURL& url,
    const std::string& id) {
  if (platform != "play")
    return PLATFORM_NOT_SUPPORTED_ON_ANDROID;

  if (id.empty())
    return NO_ID_SPECIFIED;

  banners::TrackDisplayEvent(DISPLAY_EVENT_NATIVE_APP_BANNER_REQUESTED);

  std::string id_from_app_url = ExtractQueryValueForName(url, "id");
  if (id_from_app_url.size() && id != id_from_app_url)
    return IDS_DO_NOT_MATCH;

  // Attach the chrome_inline referrer value, prefixed with "&" if the referrer
  // is non empty.
  std::string referrer = ExtractQueryValueForName(url, "referrer");
  if (!referrer.empty())
    referrer += "&";
  referrer += "playinline=chrome_inline";

  // Send the info to the Java side to get info about the app.
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jurl(
      ConvertUTF8ToJavaString(env, validated_url_.spec()));
  ScopedJavaLocalRef<jstring> jpackage(ConvertUTF8ToJavaString(env, id));
  ScopedJavaLocalRef<jstring> jreferrer(ConvertUTF8ToJavaString(env, referrer));

  // This async call will run OnAppDetailsRetrieved() when completed.
  UpdateState(State::FETCHING_NATIVE_DATA);
  Java_AppBannerManager_fetchAppDetails(
      env, java_banner_manager_, jurl, jpackage, jreferrer,
      ShortcutHelper::GetIdealHomescreenIconSizeInPx());
  return NO_ERROR_DETECTED;
}

void AppBannerManagerAndroid::CreateJavaBannerManager() {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_banner_manager_.Reset(
      Java_AppBannerManager_create(env, reinterpret_cast<intptr_t>(this)));
}

std::string AppBannerManagerAndroid::ExtractQueryValueForName(
    const GURL& url,
    const std::string& name) {
  for (net::QueryIterator it(url); !it.IsAtEnd(); it.Advance()) {
    if (it.GetKey() == name)
      return it.GetValue();
  }
  return std::string();
}

// static
jint JNI_AppBannerManager_GetHomescreenLanguageOption(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz) {
  return AppBannerSettingsHelper::GetHomescreenLanguageOption();
}

// static
ScopedJavaLocalRef<jobject>
JNI_AppBannerManager_GetJavaBannerManagerForWebContents(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jobject>& java_web_contents) {
  AppBannerManagerAndroid* manager = AppBannerManagerAndroid::FromWebContents(
      content::WebContents::FromJavaWebContents(java_web_contents));
  return manager ? ScopedJavaLocalRef<jobject>(manager->GetJavaBannerManager())
                 : ScopedJavaLocalRef<jobject>();
}

// static
void JNI_AppBannerManager_SetDaysAfterDismissAndIgnoreToTrigger(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    jint dismiss_days,
    jint ignore_days) {
  AppBannerSettingsHelper::SetDaysAfterDismissAndIgnoreToTrigger(dismiss_days,
                                                                 ignore_days);
}

// static
void JNI_AppBannerManager_SetTimeDeltaForTesting(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    jint days) {
  AppBannerManager::SetTimeDeltaForTesting(days);
}

// static
void JNI_AppBannerManager_SetTotalEngagementToTrigger(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    jdouble engagement) {
  AppBannerSettingsHelper::SetTotalEngagementToTrigger(engagement);
}

}  // namespace banners
