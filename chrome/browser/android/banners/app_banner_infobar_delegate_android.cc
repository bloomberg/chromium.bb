// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/banners/app_banner_infobar_delegate_android.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/android/shortcut_helper.h"
#include "chrome/browser/android/shortcut_info.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/webapk/webapk_install_service.h"
#include "chrome/browser/android/webapk/webapk_metrics.h"
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
    bool is_webapk) {
  DCHECK(shortcut_info);
  const GURL url = shortcut_info->url;
  if (url.is_empty())
    return false;

  auto infobar_delegate =
      base::WrapUnique(new banners::AppBannerInfoBarDelegateAndroid(
          weak_manager, std::move(shortcut_info), primary_icon, badge_icon,
          is_webapk));
  auto infobar = base::MakeUnique<AppBannerInfoBarAndroid>(
      std::move(infobar_delegate), url);
  if (!InfoBarService::FromWebContents(web_contents)
           ->AddInfoBar(std::move(infobar)))
    return false;

  return true;
}

// static
bool AppBannerInfoBarDelegateAndroid::Create(
    content::WebContents* web_contents,
    const base::string16& app_title,
    const base::android::ScopedJavaGlobalRef<jobject>& native_app_data,
    const SkBitmap& icon,
    const std::string& native_app_package_name,
    const std::string& referrer) {
  auto infobar_delegate = base::WrapUnique(new AppBannerInfoBarDelegateAndroid(
      app_title, native_app_data, icon, native_app_package_name, referrer));
  return InfoBarService::FromWebContents(web_contents)
      ->AddInfoBar(base::MakeUnique<AppBannerInfoBarAndroid>(
           std::move(infobar_delegate), native_app_data));
}

AppBannerInfoBarDelegateAndroid::~AppBannerInfoBarDelegateAndroid() {
  if (!has_user_interaction_) {
    if (!native_app_data_.is_null()) {
      TrackUserResponse(USER_RESPONSE_NATIVE_APP_IGNORED);
    } else {
      TrackUserResponse(USER_RESPONSE_WEB_APP_IGNORED);
      if (is_webapk_)
        webapk::TrackInstallEvent(webapk::INFOBAR_IGNORED);
    }
  }

  TrackDismissEvent(DISMISS_EVENT_DISMISSED);
  Java_AppBannerInfoBarDelegateAndroid_destroy(
      base::android::AttachCurrentThread(), java_delegate_);
  java_delegate_.Reset();
}

void AppBannerInfoBarDelegateAndroid::UpdateInstallState(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  if (native_app_data_.is_null() && !is_webapk_)
    return;

  ScopedJavaLocalRef<jstring> jpackage_name(
      ConvertUTF8ToJavaString(env, package_name_));
  int newState = Java_AppBannerInfoBarDelegateAndroid_determineInstallState(
      env, java_delegate_, jpackage_name);
  static_cast<AppBannerInfoBarAndroid*>(infobar())->OnInstallStateChanged(
      newState);
}

void AppBannerInfoBarDelegateAndroid::OnInstallIntentReturned(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean jis_installing) {
  DCHECK(infobar());
  DCHECK(!package_name_.empty());

  content::WebContents* web_contents =
      InfoBarService::WebContentsFromInfoBar(infobar());
  if (jis_installing) {
    AppBannerSettingsHelper::RecordBannerEvent(
        web_contents, web_contents->GetURL(), package_name_,
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
    TrackDismissEvent(DISMISS_EVENT_ERROR);
    return true;
  }

  if (!native_app_data_.is_null())
    return AcceptNativeApp(web_contents);

  if (is_webapk_)
    return AcceptWebApk(web_contents);

  return AcceptWebApp(web_contents);
}

AppBannerInfoBarDelegateAndroid::AppBannerInfoBarDelegateAndroid(
    base::WeakPtr<AppBannerManager> weak_manager,
    std::unique_ptr<ShortcutInfo> shortcut_info,
    const SkBitmap& primary_icon,
    const SkBitmap& badge_icon,
    bool is_webapk)
    : weak_manager_(weak_manager),
      app_title_(shortcut_info->name),
      shortcut_info_(std::move(shortcut_info)),
      primary_icon_(primary_icon),
      badge_icon_(badge_icon),
      has_user_interaction_(false),
      is_webapk_(is_webapk) {
  CreateJavaDelegate();
}

AppBannerInfoBarDelegateAndroid::AppBannerInfoBarDelegateAndroid(
    const base::string16& app_title,
    const base::android::ScopedJavaGlobalRef<jobject>& native_app_data,
    const SkBitmap& icon,
    const std::string& native_app_package_name,
    const std::string& referrer)
    : app_title_(app_title),
      native_app_data_(native_app_data),
      primary_icon_(icon),
      package_name_(native_app_package_name),
      referrer_(referrer),
      has_user_interaction_(false),
      is_webapk_(false) {
  DCHECK(!native_app_data_.is_null());
  DCHECK(!package_name_.empty());
  CreateJavaDelegate();
}

void AppBannerInfoBarDelegateAndroid::CreateJavaDelegate() {
  java_delegate_.Reset(Java_AppBannerInfoBarDelegateAndroid_create(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this)));
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
  TrackUserResponse(USER_RESPONSE_WEB_APP_ACCEPTED);
  AppBannerSettingsHelper::RecordBannerInstallEvent(
      web_contents, shortcut_info_->url.spec(), AppBannerSettingsHelper::WEB);

  WebApkInstallService::Get(web_contents->GetBrowserContext())
      ->InstallAsync(web_contents, *shortcut_info_, primary_icon_, badge_icon_,
                     webapk::INSTALL_SOURCE_BANNER);
  SendBannerAccepted();
  return true;
}

void AppBannerInfoBarDelegateAndroid::SendBannerAccepted() {
  if (!weak_manager_)
    return;

  weak_manager_->SendBannerAccepted();

  // Send the appinstalled event and perform install time logging. Note that
  // this is fired *before* the installation actually takes place (which can be
  // a significant amount of time later, especially if using WebAPKs).
  // TODO(mgiuca): Fire the event *after* the installation is completed.
  weak_manager_->OnInstall(!native_app_data_.is_null() /* is_native */,
                           native_app_data_.is_null()
                               ? blink::kWebDisplayModeUndefined
                               : shortcut_info_->display);
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

  if (weak_manager_)
    weak_manager_->SendBannerDismissed();

  if (native_app_data_.is_null()) {
    if (is_webapk_)
      webapk::TrackInstallEvent(webapk::INFOBAR_DISMISSED_BEFORE_INSTALLATION);
    TrackUserResponse(USER_RESPONSE_WEB_APP_DISMISSED);
    AppBannerSettingsHelper::RecordBannerDismissEvent(
        web_contents, shortcut_info_->url.spec(), AppBannerSettingsHelper::WEB);
  } else {
    DCHECK(!package_name_.empty());
    TrackUserResponse(USER_RESPONSE_NATIVE_APP_DISMISSED);
    AppBannerSettingsHelper::RecordBannerDismissEvent(
        web_contents, package_name_, AppBannerSettingsHelper::NATIVE);
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
  content::WebContents* web_contents =
      InfoBarService::WebContentsFromInfoBar(infobar());
  TabAndroid* tab = TabAndroid::FromWebContents(web_contents);
  DCHECK(tab);

  Java_AppBannerInfoBarDelegateAndroid_showAppDetails(
      base::android::AttachCurrentThread(), java_delegate_,
      tab->GetJavaObject(), native_app_data_);

  TrackDismissEvent(DISMISS_EVENT_BANNER_CLICK);
  return true;
}

}  // namespace banners
