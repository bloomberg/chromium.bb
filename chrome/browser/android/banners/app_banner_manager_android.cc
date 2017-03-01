// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/banners/app_banner_manager_android.h"

#include <memory>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/banners/app_banner_infobar_delegate_android.h"
#include "chrome/browser/android/shortcut_helper.h"
#include "chrome/browser/android/webapk/chrome_webapk_host.h"
#include "chrome/browser/android/webapk/webapk_metrics.h"
#include "chrome/browser/android/webapk/webapk_web_manifest_checker.h"
#include "chrome/browser/banners/app_banner_metrics.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/manifest/manifest_icon_downloader.h"
#include "chrome/browser/manifest/manifest_icon_selector.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/frame_navigate_params.h"
#include "jni/AppBannerManager_jni.h"
#include "net/base/url_util.h"
#include "third_party/skia/include/core/SkBitmap.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(banners::AppBannerManagerAndroid);

namespace {

std::unique_ptr<ShortcutInfo> CreateShortcutInfo(
    const GURL& manifest_url,
    const content::Manifest& manifest,
    const GURL& icon_url) {
  auto shortcut_info = base::MakeUnique<ShortcutInfo>(GURL());
  if (!manifest.IsEmpty()) {
    shortcut_info->UpdateFromManifest(manifest);
    shortcut_info->manifest_url = manifest_url;
    shortcut_info->best_primary_icon_url = icon_url;
    shortcut_info->UpdateSource(ShortcutInfo::SOURCE_APP_BANNER);
  }

  shortcut_info->ideal_splash_image_size_in_px =
      ShortcutHelper::GetIdealSplashImageSizeInPx();
  shortcut_info->minimum_splash_image_size_in_px =
      ShortcutHelper::GetMinimumSplashImageSizeInPx();
  shortcut_info->splash_image_url = ManifestIconSelector::FindBestMatchingIcon(
      manifest.icons, shortcut_info->ideal_splash_image_size_in_px,
      shortcut_info->minimum_splash_image_size_in_px,
      content::Manifest::Icon::IconPurpose::ANY);

  return shortcut_info;
}

}  // anonymous namespace

namespace banners {

AppBannerManagerAndroid::AppBannerManagerAndroid(
    content::WebContents* web_contents)
    : AppBannerManager(web_contents) {
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

bool AppBannerManagerAndroid::IsActiveForTesting(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return is_active();
}

bool AppBannerManagerAndroid::OnAppDetailsRetrieved(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& japp_data,
    const JavaParamRef<jstring>& japp_title,
    const JavaParamRef<jstring>& japp_package,
    const JavaParamRef<jstring>& jicon_url) {
  native_app_data_.Reset(japp_data);
  app_title_ = ConvertJavaStringToUTF16(env, japp_title);
  native_app_package_ = ConvertJavaStringToUTF8(env, japp_package);
  primary_icon_url_ = GURL(ConvertJavaStringToUTF8(env, jicon_url));

  if (!CheckIfShouldShowBanner())
    return false;

  return ManifestIconDownloader::Download(
      web_contents(), primary_icon_url_, GetIdealPrimaryIconSizeInPx(),
      GetMinimumPrimaryIconSizeInPx(),
      base::Bind(&AppBannerManager::OnAppIconFetched, GetWeakPtr()));
}

void AppBannerManagerAndroid::RequestAppBanner(const GURL& validated_url,
                                               bool is_debug_mode) {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!Java_AppBannerManager_isEnabledForTab(env, java_banner_manager_))
    return;

  AppBannerManager::RequestAppBanner(validated_url, is_debug_mode);
}

int AppBannerManagerAndroid::GetIdealBadgeIconSizeInPx() {
  return ShortcutHelper::GetIdealBadgeIconSizeInPx();
}

std::string AppBannerManagerAndroid::GetAppIdentifier() {
  return native_app_data_.is_null() ? AppBannerManager::GetAppIdentifier()
                                    : native_app_package_;
}

std::string AppBannerManagerAndroid::GetBannerType() {
  return native_app_data_.is_null() ? AppBannerManager::GetBannerType()
                                    : "play";
}

int AppBannerManagerAndroid::GetIdealPrimaryIconSizeInPx() {
  return ShortcutHelper::GetIdealHomescreenIconSizeInPx();
}

int AppBannerManagerAndroid::GetMinimumPrimaryIconSizeInPx() {
  return ShortcutHelper::GetMinimumHomescreenIconSizeInPx();
}

bool AppBannerManagerAndroid::IsWebAppInstalled(
    content::BrowserContext* browser_context,
    const GURL& start_url,
    const GURL& manifest_url) {
  // Returns true if a WebAPK is installed or is being installed.
  // Does not check whether a non-WebAPK web app is installed: this is detected
  // by the content settings check in AppBannerSettingsHelper::ShouldShowBanner
  // (due to the lack of an API to detect what is and isn't on the Android
  // homescreen). This method will still detect the presence of a WebAPK even if
  // Chrome's data is cleared.
  return ShortcutHelper::IsWebApkInstalled(browser_context, start_url,
                                           manifest_url);
}

InstallableParams AppBannerManagerAndroid::ParamsToPerformInstallableCheck() {
  InstallableParams params =
      AppBannerManager::ParamsToPerformInstallableCheck();

  if (can_install_webapk_) {
    params.ideal_badge_icon_size_in_px = GetIdealBadgeIconSizeInPx();
    params.minimum_badge_icon_size_in_px = GetIdealBadgeIconSizeInPx();
    params.fetch_valid_badge_icon = true;
  }

  return params;
}

void AppBannerManagerAndroid::PerformInstallableCheck() {
  // Check if the manifest prefers that we show a native app banner. If so, call
  // to Java to verify the details.
  if (manifest_.prefer_related_applications &&
      manifest_.related_applications.size()) {
    for (const auto& application : manifest_.related_applications) {
      std::string platform = base::UTF16ToUTF8(application.platform.string());
      std::string id = base::UTF16ToUTF8(application.id.string());
      if (CanHandleNonWebApp(platform, application.url, id))
        return;
    }
    Stop();
  }

  if (can_install_webapk_) {
    if (!AreWebManifestUrlsWebApkCompatible(manifest_)) {
      ReportStatus(web_contents(), URL_NOT_SUPPORTED_FOR_WEBAPK);
      Stop();
      return;
    }
  }

  // No native app banner was requested. Continue checking for a web app banner.
  AppBannerManager::PerformInstallableCheck();
}

void AppBannerManagerAndroid::OnDidPerformInstallableCheck(
    const InstallableData& data) {
  if (data.badge_icon && !data.badge_icon->drawsNothing()) {
    DCHECK(!data.badge_icon_url.is_empty());

    badge_icon_url_ = data.badge_icon_url;
    badge_icon_.reset(new SkBitmap(*data.badge_icon));
  }

  AppBannerManager::OnDidPerformInstallableCheck(data);
}

void AppBannerManagerAndroid::OnAppIconFetched(const SkBitmap& bitmap) {
  if (bitmap.drawsNothing()) {
    ReportStatus(web_contents(), NO_ICON_AVAILABLE);
    Stop();
  }

  if (!is_active())
    return;

  primary_icon_.reset(new SkBitmap(bitmap));
  SendBannerPromptRequest();
}

void AppBannerManagerAndroid::ResetCurrentPageData() {
  AppBannerManager::ResetCurrentPageData();
  native_app_data_.Reset();
  native_app_package_ = "";
}

void AppBannerManagerAndroid::ShowBanner() {
  content::WebContents* contents = web_contents();
  DCHECK(contents);

  if (native_app_data_.is_null()) {
    // TODO(zpeng): Add badge to WebAPK installation flow.
    if (AppBannerInfoBarDelegateAndroid::Create(
            contents, GetWeakPtr(), app_title_,
            CreateShortcutInfo(manifest_url_, manifest_, primary_icon_url_),
            std::move(primary_icon_), event_request_id(),
            webapk::INSTALL_SOURCE_BANNER)) {
      RecordDidShowBanner("AppBanner.WebApp.Shown");
      TrackDisplayEvent(DISPLAY_EVENT_WEB_APP_BANNER_CREATED);
      ReportStatus(contents, SHOWING_WEB_APP_BANNER);
    } else {
      ReportStatus(contents, FAILED_TO_CREATE_BANNER);
    }
  } else {
    if (AppBannerInfoBarDelegateAndroid::Create(
            contents, app_title_, native_app_data_, std::move(primary_icon_),
            native_app_package_, referrer_, event_request_id())) {
      RecordDidShowBanner("AppBanner.NativeApp.Shown");
      TrackDisplayEvent(DISPLAY_EVENT_NATIVE_APP_BANNER_CREATED);
      ReportStatus(contents, SHOWING_NATIVE_APP_BANNER);
    } else {
      ReportStatus(contents, FAILED_TO_CREATE_BANNER);
    }
  }
}

bool AppBannerManagerAndroid::CanHandleNonWebApp(const std::string& platform,
                                                 const GURL& url,
                                                 const std::string& id) {
  if (!CheckPlatformAndId(platform, id))
    return false;

  banners::TrackDisplayEvent(DISPLAY_EVENT_NATIVE_APP_BANNER_REQUESTED);

  // Send the info to the Java side to get info about the app.
  JNIEnv* env = base::android::AttachCurrentThread();
  if (java_banner_manager_.is_null())
    return false;

  std::string id_from_app_url = ExtractQueryValueForName(url, "id");
  if (id_from_app_url.size() && id != id_from_app_url) {
    ReportStatus(web_contents(), IDS_DO_NOT_MATCH);
    return false;
  }

  // Attach the chrome_inline referrer value, prefixed with "&" if the referrer
  // is non empty.
  std::string referrer = ExtractQueryValueForName(url, "referrer");
  if (!referrer.empty())
    referrer += "&";
  referrer += "playinline=chrome_inline";

  ScopedJavaLocalRef<jstring> jurl(
      ConvertUTF8ToJavaString(env, validated_url_.spec()));
  ScopedJavaLocalRef<jstring> jpackage(ConvertUTF8ToJavaString(env, id));
  ScopedJavaLocalRef<jstring> jreferrer(ConvertUTF8ToJavaString(env, referrer));
  Java_AppBannerManager_fetchAppDetails(env, java_banner_manager_, jurl,
                                        jpackage, jreferrer,
                                        GetIdealPrimaryIconSizeInPx());
  return true;
}

void AppBannerManagerAndroid::CreateJavaBannerManager() {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_banner_manager_.Reset(
      Java_AppBannerManager_create(env, reinterpret_cast<intptr_t>(this)));
}

bool AppBannerManagerAndroid::CheckPlatformAndId(const std::string& platform,
                                                 const std::string& id) {
  const bool correct_platform = (platform == "play");
  if (correct_platform && !id.empty())
    return true;
  ReportStatus(
      web_contents(),
      correct_platform ? NO_ID_SPECIFIED : PLATFORM_NOT_SUPPORTED_ON_ANDROID);
  return false;
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
bool AppBannerManagerAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
jint GetHomescreenLanguageOption(JNIEnv* env,
                                 const JavaParamRef<jclass>& clazz) {
  return AppBannerSettingsHelper::GetHomescreenLanguageOption();
}

// static
ScopedJavaLocalRef<jobject> GetJavaBannerManagerForWebContents(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jobject>& java_web_contents) {
  AppBannerManagerAndroid* manager = AppBannerManagerAndroid::FromWebContents(
      content::WebContents::FromJavaWebContents(java_web_contents));
  return manager ? ScopedJavaLocalRef<jobject>(manager->GetJavaBannerManager())
                 : ScopedJavaLocalRef<jobject>();
}

// static
void SetDaysAfterDismissAndIgnoreToTrigger(JNIEnv* env,
                                           const JavaParamRef<jclass>& clazz,
                                           jint dismiss_days,
                                           jint ignore_days) {
  AppBannerSettingsHelper::SetDaysAfterDismissAndIgnoreToTrigger(dismiss_days,
                                                                 ignore_days);
}

// static
void SetTimeDeltaForTesting(JNIEnv* env,
                            const JavaParamRef<jclass>& clazz,
                            jint days) {
  AppBannerManager::SetTimeDeltaForTesting(days);
}

// static
void SetTotalEngagementToTrigger(JNIEnv* env,
                                 const JavaParamRef<jclass>& clazz,
                                 jdouble engagement) {
  AppBannerSettingsHelper::SetTotalEngagementToTrigger(engagement);
}

}  // namespace banners
