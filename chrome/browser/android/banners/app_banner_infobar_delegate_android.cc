// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/banners/app_banner_infobar_delegate_android.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/android/shortcut_helper.h"
#include "chrome/browser/android/shortcut_info.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/webapk/webapk_install_service.h"
#include "chrome/browser/banners/app_banner_manager.h"
#include "chrome/browser/banners/app_banner_metrics.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/android/infobars/app_banner_infobar_android.h"
#include "components/rappor/public/rappor_utils.h"
#include "components/rappor/rappor_service_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/manifest.h"
#include "jni/AppBannerInfoBarDelegateAndroid_jni.h"
#include "ui/gfx/android/java_bitmap.h"
#include "url/gurl.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace banners {

// static
bool AppBannerInfoBarDelegateAndroid::Create(
    content::WebContents* web_contents,
    base::WeakPtr<AppBannerManager> weak_manager,
    std::unique_ptr<ShortcutInfo> shortcut_info,
    const SkBitmap& primary_icon,
    const SkBitmap& badge_icon,
    bool is_webapk,
    webapk::InstallSource webapk_install_source) {
  DCHECK(shortcut_info);
  const GURL url = shortcut_info->url;
  if (url.is_empty())
    return false;

  auto infobar_delegate =
      base::WrapUnique(new banners::AppBannerInfoBarDelegateAndroid(
          weak_manager, std::move(shortcut_info), primary_icon, badge_icon,
          is_webapk, webapk_install_source));
  auto* raw_delegate = infobar_delegate.get();
  auto infobar = base::MakeUnique<AppBannerInfoBarAndroid>(
      std::move(infobar_delegate), url, is_webapk);
  if (!InfoBarService::FromWebContents(web_contents)
           ->AddInfoBar(std::move(infobar)))
    return false;

  if (is_webapk) {
    if (webapk_install_source == webapk::INSTALL_SOURCE_MENU) {
      webapk::TrackInstallInfoBarShown(
          webapk::WEBAPK_INFOBAR_SHOWN_FROM_MENU);
      raw_delegate->Accept();
    } else {
      webapk::TrackInstallInfoBarShown(
          webapk::WEBAPK_INFOBAR_SHOWN_FROM_BANNER);
    }
  }

  return true;
}

// static
bool AppBannerInfoBarDelegateAndroid::Create(
    content::WebContents* web_contents,
    const base::string16& app_title,
    const base::android::ScopedJavaGlobalRef<jobject>& native_app_data,
    const SkBitmap& icon,
    const std::string& native_app_package,
    const std::string& referrer) {
  auto infobar_delegate = base::WrapUnique(new AppBannerInfoBarDelegateAndroid(
      app_title, native_app_data, icon, native_app_package, referrer));
  return InfoBarService::FromWebContents(web_contents)
      ->AddInfoBar(base::MakeUnique<AppBannerInfoBarAndroid>(
           std::move(infobar_delegate), native_app_data));
}

AppBannerInfoBarDelegateAndroid::~AppBannerInfoBarDelegateAndroid() {
  weak_ptr_factory_.InvalidateWeakPtrs();

  if (!has_user_interaction_) {
    if (!native_app_data_.is_null()) {
      TrackUserResponse(USER_RESPONSE_NATIVE_APP_IGNORED);
    } else {
      if (TriggeredFromBanner())
        TrackUserResponse(USER_RESPONSE_WEB_APP_IGNORED);
      if (is_webapk_)
        webapk::TrackInstallEvent(webapk::INFOBAR_IGNORED);
    }
  }

  if (TriggeredFromBanner())
    TrackDismissEvent(DISMISS_EVENT_DISMISSED);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AppBannerInfoBarDelegateAndroid_destroy(env, java_delegate_);
  java_delegate_.Reset();
}

void AppBannerInfoBarDelegateAndroid::UpdateInstallState(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  if (native_app_data_.is_null() && !is_webapk_)
    return;

  int newState = Java_AppBannerInfoBarDelegateAndroid_determineInstallState(
      env, java_delegate_, native_app_data_);
  static_cast<AppBannerInfoBarAndroid*>(infobar())
      ->OnInstallStateChanged(newState);
}

void AppBannerInfoBarDelegateAndroid::OnInstallIntentReturned(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean jis_installing) {
  DCHECK(infobar());

  content::WebContents* web_contents =
      InfoBarService::WebContentsFromInfoBar(infobar());
  if (jis_installing) {
    AppBannerSettingsHelper::RecordBannerEvent(
        web_contents,
        web_contents->GetURL(),
        native_app_package_,
        AppBannerSettingsHelper::APP_BANNER_EVENT_DID_ADD_TO_HOMESCREEN,
        AppBannerManager::GetCurrentTime());

    TrackInstallEvent(INSTALL_EVENT_NATIVE_APP_INSTALL_STARTED);
    rappor::SampleDomainAndRegistryFromGURL(g_browser_process->rappor_service(),
                                            "AppBanner.NativeApp.Installed",
                                            web_contents->GetURL());
  }

  UpdateInstallState(env, obj);
}

void AppBannerInfoBarDelegateAndroid::OnInstallFinished(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean success) {
  DCHECK(infobar());

  if (success) {
    TrackInstallEvent(INSTALL_EVENT_NATIVE_APP_INSTALL_COMPLETED);
    UpdateInstallState(env, obj);
  } else if (infobar()->owner()) {
    TrackDismissEvent(DISMISS_EVENT_INSTALL_TIMEOUT);
    infobar()->owner()->RemoveInfoBar(infobar());
  }
}

bool AppBannerInfoBarDelegateAndroid::Accept() {
  has_user_interaction_ = true;

  content::WebContents* web_contents =
      InfoBarService::WebContentsFromInfoBar(infobar());
  if (!web_contents) {
    if (TriggeredFromBanner())
      TrackDismissEvent(DISMISS_EVENT_ERROR);
    return true;
  }

  if (!native_app_data_.is_null())
    return AcceptNativeApp(web_contents);

  if (is_webapk_)
    return AcceptWebApk(web_contents);

  return AcceptWebApp(web_contents);
}

void AppBannerInfoBarDelegateAndroid::UpdateStateForInstalledWebAPK(
    const std::string& webapk_package_name) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> java_webapk_package_name =
      base::android::ConvertUTF8ToJavaString(env, webapk_package_name);
  Java_AppBannerInfoBarDelegateAndroid_setWebApkInstallingState(
      env, java_delegate_, false);
  Java_AppBannerInfoBarDelegateAndroid_setWebApkPackageName(
      env, java_delegate_, java_webapk_package_name);
  UpdateInstallState(env, nullptr);
  install_state_ = INSTALLED;
}

AppBannerInfoBarDelegateAndroid::AppBannerInfoBarDelegateAndroid(
    base::WeakPtr<AppBannerManager> weak_manager,
    std::unique_ptr<ShortcutInfo> shortcut_info,
    const SkBitmap& primary_icon,
    const SkBitmap& badge_icon,
    bool is_webapk,
    webapk::InstallSource webapk_install_source)
    : weak_manager_(weak_manager),
      app_title_(shortcut_info->name),
      shortcut_info_(std::move(shortcut_info)),
      primary_icon_(primary_icon),
      badge_icon_(badge_icon),
      has_user_interaction_(false),
      is_webapk_(is_webapk),
      install_state_(INSTALL_NOT_STARTED),
      webapk_install_source_(webapk_install_source),
      weak_ptr_factory_(this) {
  CreateJavaDelegate();
}

AppBannerInfoBarDelegateAndroid::AppBannerInfoBarDelegateAndroid(
    const base::string16& app_title,
    const base::android::ScopedJavaGlobalRef<jobject>& native_app_data,
    const SkBitmap& icon,
    const std::string& native_app_package,
    const std::string& referrer)
    : app_title_(app_title),
      native_app_data_(native_app_data),
      primary_icon_(icon),
      native_app_package_(native_app_package),
      referrer_(referrer),
      has_user_interaction_(false),
      weak_ptr_factory_(this) {
  DCHECK(!native_app_data_.is_null());
  CreateJavaDelegate();
}

void AppBannerInfoBarDelegateAndroid::CreateJavaDelegate() {
  java_delegate_.Reset(Java_AppBannerInfoBarDelegateAndroid_create(
      base::android::AttachCurrentThread(),
      reinterpret_cast<intptr_t>(this)));
}

bool AppBannerInfoBarDelegateAndroid::AcceptNativeApp(
    content::WebContents* web_contents) {
  TrackUserResponse(USER_RESPONSE_NATIVE_APP_ACCEPTED);
  JNIEnv* env = base::android::AttachCurrentThread();

  TabAndroid* tab = TabAndroid::FromWebContents(web_contents);
  DCHECK(tab);
  ScopedJavaLocalRef<jstring> jreferrer(
      ConvertUTF8ToJavaString(env, referrer_));

  bool was_opened =
      Java_AppBannerInfoBarDelegateAndroid_installOrOpenNativeApp(
          env, java_delegate_, tab->GetJavaObject(),
          native_app_data_, jreferrer);

  if (was_opened)
    TrackDismissEvent(DISMISS_EVENT_APP_OPEN);
  else
    TrackInstallEvent(INSTALL_EVENT_NATIVE_APP_INSTALL_TRIGGERED);

  SendBannerAccepted();
  return was_opened;
}

bool AppBannerInfoBarDelegateAndroid::AcceptWebApp(
    content::WebContents* web_contents) {
  TrackUserResponse(USER_RESPONSE_WEB_APP_ACCEPTED);

  AppBannerSettingsHelper::RecordBannerInstallEvent(
      web_contents, shortcut_info_->url.spec(), AppBannerSettingsHelper::WEB);

  ShortcutHelper::AddToLauncherWithSkBitmap(web_contents, *shortcut_info_,
                                            primary_icon_);

  SendBannerAccepted();
  return true;
}

bool AppBannerInfoBarDelegateAndroid::AcceptWebApk(
    content::WebContents* web_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();

  // If the WebAPK is installed and the "Open" button is clicked, open the
  // WebAPK. Do not send a BannerAccepted message.
  if (install_state_ == INSTALLED) {
    Java_AppBannerInfoBarDelegateAndroid_openWebApk(env, java_delegate_);
    webapk::TrackUserAction(webapk::USER_ACTION_INSTALLED_OPEN);
    return true;
  }

  install_state_ = INSTALLING;
  webapk::TrackInstallSource(webapk_install_source_);

  if (TriggeredFromBanner()) {
    TrackUserResponse(USER_RESPONSE_WEB_APP_ACCEPTED);
    AppBannerSettingsHelper::RecordBannerInstallEvent(
        web_contents, shortcut_info_->url.spec(), AppBannerSettingsHelper::WEB);
  } else if (is_webapk_) {
    // This branch will be entered if we are a WebAPK that was triggered from
    // the add to homescreen menu item. Manually record the app banner added to
    // homescreen event in this case. Don't call
    // AppBannerSettingsHelper::RecordBannerInstallEvent as that records
    // banner-specific metrics in addition to this event.
    AppBannerSettingsHelper::RecordBannerEvent(
        web_contents, web_contents->GetLastCommittedURL(),
        shortcut_info_->url.spec(),
        AppBannerSettingsHelper::APP_BANNER_EVENT_DID_ADD_TO_HOMESCREEN,
        AppBannerManager::GetCurrentTime());
  }

  Java_AppBannerInfoBarDelegateAndroid_setWebApkInstallingState(
      env, java_delegate_, true);
  UpdateInstallState(env, nullptr);
  WebApkInstallService::FinishCallback callback =
      base::Bind(&AppBannerInfoBarDelegateAndroid::OnWebApkInstallFinished,
                 weak_ptr_factory_.GetWeakPtr());
  ShortcutHelper::InstallWebApkWithSkBitmap(
      web_contents, *shortcut_info_, primary_icon_, badge_icon_, callback);

  SendBannerAccepted();

  // Prevent the infobar from disappearing, because the infobar will show
  // "Adding" during the installation process.
  return false;
}

bool AppBannerInfoBarDelegateAndroid::TriggeredFromBanner() const {
  return !is_webapk_ || webapk_install_source_ == webapk::INSTALL_SOURCE_BANNER;
}

void AppBannerInfoBarDelegateAndroid::SendBannerAccepted() {
  if (!weak_manager_)
    return;

  if (TriggeredFromBanner())
    weak_manager_->SendBannerAccepted();

  // Send the appinstalled event. Note that this is fired *before* the
  // installation actually takes place (which can be a significant amount of
  // time later, especially if using WebAPKs).
  // TODO(mgiuca): Fire the event *after* the installation is completed.
  weak_manager_->OnInstall();
}

void AppBannerInfoBarDelegateAndroid::OnWebApkInstallFinished(
    WebApkInstallResult result,
    bool relax_updates,
    const std::string& webapk_package_name) {
  if (result != WebApkInstallResult::SUCCESS) {
    OnWebApkInstallFailed(result);
    return;
  }
  UpdateStateForInstalledWebAPK(webapk_package_name);
}

void AppBannerInfoBarDelegateAndroid::OnWebApkInstallFailed(
    WebApkInstallResult result) {
  if (!infobar())
    return;

  // If the install didn't definitely fail, we don't add a shortcut. This could
  // happen if Play was busy with another install and this one is still queued
  // (and hence might succeed in the future).
  if (result == WebApkInstallResult::FAILURE) {
    content::WebContents* web_contents =
        InfoBarService::WebContentsFromInfoBar(infobar());
    // Add webapp shortcut to the homescreen.
    ShortcutHelper::AddToLauncherWithSkBitmap(web_contents, *shortcut_info_,
                                              primary_icon_);
  }

  infobar()->RemoveSelf();
}

void AppBannerInfoBarDelegateAndroid::TrackWebApkInstallationDismissEvents(
    InstallState install_state) {
  if (install_state == INSTALL_NOT_STARTED) {
    webapk::TrackInstallEvent(webapk::INFOBAR_DISMISSED_BEFORE_INSTALLATION);
  } else if (install_state == INSTALLING) {
    webapk::TrackInstallEvent(webapk::INFOBAR_DISMISSED_DURING_INSTALLATION);
  } else if (install_state == INSTALLED) {
    // If WebAPK is installed from this banner, TrackInstallEvent() is called in
    // WebApkInstaller::OnResult().
    webapk::TrackUserAction(webapk::USER_ACTION_INSTALLED_OPEN_DISMISS);
  }
}

infobars::InfoBarDelegate::InfoBarIdentifier
AppBannerInfoBarDelegateAndroid::GetIdentifier() const {
  return APP_BANNER_INFOBAR_DELEGATE_ANDROID;
}

gfx::Image AppBannerInfoBarDelegateAndroid::GetIcon() const {
  return gfx::Image::CreateFrom1xBitmap(primary_icon_);
}

void AppBannerInfoBarDelegateAndroid::InfoBarDismissed() {
  has_user_interaction_ = true;

  content::WebContents* web_contents =
      InfoBarService::WebContentsFromInfoBar(infobar());

  if (weak_manager_ && TriggeredFromBanner())
    weak_manager_->SendBannerDismissed();

  if (native_app_data_.is_null()) {
    if (is_webapk_)
      TrackWebApkInstallationDismissEvents(install_state_);
    if (TriggeredFromBanner()) {
      TrackUserResponse(USER_RESPONSE_WEB_APP_DISMISSED);
      AppBannerSettingsHelper::RecordBannerDismissEvent(
          web_contents, shortcut_info_->url.spec(),
          AppBannerSettingsHelper::WEB);
    }
  } else {
    TrackUserResponse(USER_RESPONSE_NATIVE_APP_DISMISSED);
    AppBannerSettingsHelper::RecordBannerDismissEvent(
        web_contents, native_app_package_, AppBannerSettingsHelper::NATIVE);
  }
}

base::string16 AppBannerInfoBarDelegateAndroid::GetMessageText() const {
  return app_title_;
}

int AppBannerInfoBarDelegateAndroid::GetButtons() const {
  return BUTTON_OK;
}

bool AppBannerInfoBarDelegateAndroid::LinkClicked(
    WindowOpenDisposition disposition) {
  if (native_app_data_.is_null())
    return false;

  // Try to show the details for the native app.
  JNIEnv* env = base::android::AttachCurrentThread();

  content::WebContents* web_contents =
      InfoBarService::WebContentsFromInfoBar(infobar());
  TabAndroid* tab = TabAndroid::FromWebContents(web_contents);
  DCHECK(tab);

  Java_AppBannerInfoBarDelegateAndroid_showAppDetails(
      env, java_delegate_, tab->GetJavaObject(), native_app_data_);

  TrackDismissEvent(DISMISS_EVENT_BANNER_CLICK);
  return true;
}

}  // namespace banners
