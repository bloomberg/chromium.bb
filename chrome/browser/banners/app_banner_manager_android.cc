// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/app_banner_manager_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/shortcut_helper.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/webapk/chrome_webapk_host.h"
#include "chrome/browser/android/webapk/webapk_web_manifest_checker.h"
#include "chrome/browser/banners/app_banner_metrics.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/banners/app_banner_ui_delegate_android.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/common/chrome_features.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "content/public/browser/manifest_icon_downloader.h"
#include "content/public/browser/web_contents.h"
#include "jni/AppBannerManager_jni.h"
#include "net/base/url_util.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;

namespace {

// Returns a pointer to the InstallableAmbientBadgeInfoBar if it is currently
// showing. Otherwise returns nullptr.
infobars::InfoBar* GetVisibleAmbientBadgeInfoBar(
    InfoBarService* infobar_service) {
  for (size_t i = 0; i < infobar_service->infobar_count(); ++i) {
    infobars::InfoBar* infobar = infobar_service->infobar_at(i);
    if (infobar->delegate()->GetIdentifier() ==
        InstallableAmbientBadgeInfoBarDelegate::
            INSTALLABLE_AMBIENT_BADGE_INFOBAR_DELEGATE) {
      return infobar;
    }
  }
  return nullptr;
}

}  // anonymous namespace

namespace banners {

AppBannerManagerAndroid::AppBannerManagerAndroid(
    content::WebContents* web_contents)
    : AppBannerManager(web_contents) {
  can_install_webapk_ = ChromeWebApkHost::CanInstallWebApk();
  CreateJavaBannerManager(web_contents);
}

AppBannerManagerAndroid::~AppBannerManagerAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AppBannerManager_destroy(env, java_banner_manager_);
  java_banner_manager_.Reset();
}

const base::android::ScopedJavaLocalRef<jobject>
AppBannerManagerAndroid::GetJavaBannerManager() const {
  return base::android::ScopedJavaLocalRef<jobject>(java_banner_manager_);
}

base::android::ScopedJavaLocalRef<jobject>
AppBannerManagerAndroid::GetAddToHomescreenDialogForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobj) {
  return ui_delegate_ ? ui_delegate_->GetAddToHomescreenDialogForTesting()
                      : nullptr;
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
      base::BindOnce(&AppBannerManagerAndroid::OnNativeAppIconFetched,
                     weak_factory_.GetWeakPtr()));
}

void AppBannerManagerAndroid::RequestAppBanner(const GURL& validated_url) {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!Java_AppBannerManager_isEnabledForTab(env, java_banner_manager_))
    return;

  AppBannerManager::RequestAppBanner(validated_url);
}

void AppBannerManagerAndroid::AddToHomescreenFromBadge() {
  ShowBannerUi(InstallableMetrics::GetInstallSource(
      web_contents(), InstallTrigger::AMBIENT_BADGE));

  // Close our bindings to ensure that any existing beforeinstallprompt events
  // cannot trigger add to home screen (which would cause a crash). If the
  // banner is dismissed, the event will be resent.
  ResetBindings();
}

void AppBannerManagerAndroid::BadgeDismissed() {
  banners::TrackDismissEvent(banners::DISMISS_EVENT_AMBIENT_INFOBAR_DISMISSED);

  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents(), validated_url_, GetAppIdentifier(),
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_BLOCK, GetCurrentTime());
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
  return ShortcutHelper::IsWebApkInstalled(web_contents->GetBrowserContext(),
                                           start_url, manifest_url);
}

InstallableParams
AppBannerManagerAndroid::ParamsToPerformInstallableWebAppCheck() {
  InstallableParams params =
      AppBannerManager::ParamsToPerformInstallableWebAppCheck();
  params.valid_badge_icon = can_install_webapk_;

  return params;
}

void AppBannerManagerAndroid::PerformInstallableChecks() {
  if (ShouldPerformInstallableNativeAppCheck())
    PerformInstallableNativeAppCheck();
  else
    PerformInstallableWebAppCheck();
}

void AppBannerManagerAndroid::PerformInstallableWebAppCheck() {
  if (can_install_webapk_ && !AreWebManifestUrlsWebApkCompatible(manifest_)) {
    Stop(URL_NOT_SUPPORTED_FOR_WEBAPK);
    return;
  }
  AppBannerManager::PerformInstallableWebAppCheck();
}

void AppBannerManagerAndroid::OnDidPerformInstallableWebAppCheck(
    const InstallableData& data) {
  if (data.badge_icon && !data.badge_icon->drawsNothing()) {
    DCHECK(!data.badge_icon_url.is_empty());

    badge_icon_url_ = data.badge_icon_url;
    badge_icon_ = *data.badge_icon;
  }

  AppBannerManager::OnDidPerformInstallableWebAppCheck(data);
}

void AppBannerManagerAndroid::ResetCurrentPageData() {
  AppBannerManager::ResetCurrentPageData();
  native_app_data_.Reset();
  native_app_package_ = "";
  ui_delegate_ = nullptr;
}

void AppBannerManagerAndroid::ShowBannerUi(WebappInstallSource install_source) {
  content::WebContents* contents = web_contents();
  DCHECK(contents);

  if (native_app_data_.is_null()) {
    ui_delegate_ = AppBannerUiDelegateAndroid::Create(
        weak_factory_.GetWeakPtr(),
        ShortcutHelper::CreateShortcutInfo(manifest_url_, manifest_,
                                           primary_icon_url_, badge_icon_url_),
        primary_icon_, badge_icon_, install_source, can_install_webapk_);
  } else {
    ui_delegate_ = AppBannerUiDelegateAndroid::Create(
        weak_factory_.GetWeakPtr(), native_app_title_,
        base::android::ScopedJavaLocalRef<jobject>(native_app_data_),
        primary_icon_, native_app_package_);
  }

  HideAmbientBadge();

  if (ui_delegate_->ShowDialog()) {
    if (native_app_data_.is_null()) {
      RecordDidShowBanner("AppBanner.WebApp.Shown");
      TrackDisplayEvent(DISPLAY_EVENT_WEB_APP_BANNER_CREATED);
      ReportStatus(SHOWING_WEB_APP_BANNER);
    } else {
      RecordDidShowBanner("AppBanner.NativeApp.Shown");
      TrackDisplayEvent(DISPLAY_EVENT_NATIVE_APP_BANNER_CREATED);
      ReportStatus(SHOWING_NATIVE_APP_BANNER);
    }
  } else {
    ReportStatus(FAILED_TO_CREATE_BANNER);
  }
}

void AppBannerManagerAndroid::CreateJavaBannerManager(
    content::WebContents* web_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();
  TabAndroid* tab = TabAndroid::FromWebContents(web_contents);
  base::android::ScopedJavaLocalRef<jobject> jtab(tab ? tab->GetJavaObject()
                                                      : nullptr);
  java_banner_manager_.Reset(Java_AppBannerManager_create(
      env, jtab, reinterpret_cast<intptr_t>(this)));
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

bool AppBannerManagerAndroid::ShouldPerformInstallableNativeAppCheck() {
  return manifest_.prefer_related_applications &&
         manifest_.related_applications.size() &&
         !java_banner_manager_.is_null();
}

void AppBannerManagerAndroid::PerformInstallableNativeAppCheck() {
  DCHECK(ShouldPerformInstallableNativeAppCheck());
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
  base::android::ScopedJavaLocalRef<jstring> jurl(
      ConvertUTF8ToJavaString(env, validated_url_.spec()));
  base::android::ScopedJavaLocalRef<jstring> jpackage(
      ConvertUTF8ToJavaString(env, id));
  base::android::ScopedJavaLocalRef<jstring> jreferrer(
      ConvertUTF8ToJavaString(env, referrer));

  // This async call will run OnAppDetailsRetrieved() when completed.
  UpdateState(State::FETCHING_NATIVE_DATA);
  Java_AppBannerManager_fetchAppDetails(
      env, java_banner_manager_, jurl, jpackage, jreferrer,
      ShortcutHelper::GetIdealHomescreenIconSizeInPx());
  return NO_ERROR_DETECTED;
}

void AppBannerManagerAndroid::OnNativeAppIconFetched(const SkBitmap& bitmap) {
  if (bitmap.drawsNothing()) {
    Stop(NO_ICON_AVAILABLE);
    return;
  }

  primary_icon_ = bitmap;

  // If we triggered the installability check on page load, then it's possible
  // we don't have enough engagement yet. If that's the case, return here but
  // don't call Terminate(). We wait for OnEngagementEvent to tell us that we
  // should trigger.
  if (!HasSufficientEngagement()) {
    UpdateState(State::PENDING_ENGAGEMENT);
    return;
  }

  SendBannerPromptRequest();
}

base::string16 AppBannerManagerAndroid::GetAppName() const {
  if (native_app_data_.is_null()) {
    // Prefer the short name if it's available. It's guaranteed that at least
    // one of these is non-empty.
    return manifest_.short_name.string().empty()
               ? manifest_.name.string()
               : manifest_.short_name.string();
  }

  return native_app_title_;
}

void AppBannerManagerAndroid::MaybeShowAmbientBadge() {
  if (!base::FeatureList::IsEnabled(features::kInstallableAmbientBadgeInfoBar))
    return;

  // Do not show the ambient badge if it was recently dismissed.
  if (AppBannerSettingsHelper::WasBannerRecentlyBlocked(
          web_contents(), validated_url_, GetAppIdentifier(),
          GetCurrentTime())) {
    return;
  }

  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents());
  if (GetVisibleAmbientBadgeInfoBar(infobar_service) == nullptr) {
    InstallableAmbientBadgeInfoBarDelegate::Create(
        web_contents(), weak_factory_.GetWeakPtr(), GetAppName(), primary_icon_,
        manifest_.start_url);
  }
}

void AppBannerManagerAndroid::HideAmbientBadge() {
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents());
  infobars::InfoBar* ambient_badge_infobar =
      GetVisibleAmbientBadgeInfoBar(infobar_service);

  if (ambient_badge_infobar)
    infobar_service->RemoveInfoBar(ambient_badge_infobar);
}

bool AppBannerManagerAndroid::IsSupportedAppPlatform(
    const base::string16& platform) const {
  // TODO(https://crbug.com/949430): Implement for Android apps.
  return false;
}

bool AppBannerManagerAndroid::IsRelatedAppInstalled(
    const blink::Manifest::RelatedApplication& related_app) const {
  // TODO(https://crbug.com/949430): Implement for Android apps.
  return false;
}

base::WeakPtr<AppBannerManager> AppBannerManagerAndroid::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void AppBannerManagerAndroid::InvalidateWeakPtrs() {
  weak_factory_.InvalidateWeakPtrs();
}

// static
AppBannerManager* AppBannerManager::FromWebContents(
    content::WebContents* web_contents) {
  return AppBannerManagerAndroid::FromWebContents(web_contents);
}

// static
jint JNI_AppBannerManager_GetHomescreenLanguageOption(JNIEnv* env) {
  return AppBannerSettingsHelper::GetHomescreenLanguageOption();
}

// static
base::android::ScopedJavaLocalRef<jobject>
JNI_AppBannerManager_GetJavaBannerManagerForWebContents(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_web_contents) {
  AppBannerManagerAndroid* manager = AppBannerManagerAndroid::FromWebContents(
      content::WebContents::FromJavaWebContents(java_web_contents));
  return manager ? manager->GetJavaBannerManager()
                 : base::android::ScopedJavaLocalRef<jobject>();
}

// static
void JNI_AppBannerManager_SetDaysAfterDismissAndIgnoreToTrigger(
    JNIEnv* env,
    jint dismiss_days,
    jint ignore_days) {
  AppBannerSettingsHelper::SetDaysAfterDismissAndIgnoreToTrigger(dismiss_days,
                                                                 ignore_days);
}

// static
void JNI_AppBannerManager_SetTimeDeltaForTesting(
    JNIEnv* env,
    jint days) {
  AppBannerManager::SetTimeDeltaForTesting(days);
}

// static
void JNI_AppBannerManager_SetTotalEngagementToTrigger(
    JNIEnv* env,
    jdouble engagement) {
  AppBannerSettingsHelper::SetTotalEngagementToTrigger(engagement);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AppBannerManagerAndroid)

}  // namespace banners
