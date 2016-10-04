// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/banners/app_banner_infobar_delegate_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/guid.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/shortcut_helper.h"
#include "chrome/browser/android/shortcut_info.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/webapk/webapk_installer.h"
#include "chrome/browser/banners/app_banner_manager.h"
#include "chrome/browser/banners/app_banner_metrics.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/android/infobars/app_banner_infobar_android.h"
#include "chrome/common/render_messages.h"
#include "components/rappor/rappor_utils.h"
#include "content/public/browser/render_frame_host.h"
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

namespace {

bool IsInfoEmpty(const std::unique_ptr<ShortcutInfo>& info) {
  return !info || info->url.is_empty();
}

}  // anonymous namespace

namespace banners {

// static
bool AppBannerInfoBarDelegateAndroid::Create(
    content::WebContents* web_contents,
    base::WeakPtr<AppBannerManager> weak_manager,
    const base::string16& app_title,
    std::unique_ptr<ShortcutInfo> shortcut_info,
    std::unique_ptr<SkBitmap> icon,
    int event_request_id,
    bool is_webapk,
    webapk::InstallSource webapk_install_source) {
  const GURL& url = shortcut_info->url;
  auto infobar_delegate =
      base::WrapUnique(new banners::AppBannerInfoBarDelegateAndroid(
          weak_manager, app_title, std::move(shortcut_info), std::move(icon),
          event_request_id, is_webapk, webapk_install_source));
  auto raw_delegate = infobar_delegate.get();
  auto infobar = base::MakeUnique<AppBannerInfoBarAndroid>(
      std::move(infobar_delegate), url, is_webapk);
  if (!InfoBarService::FromWebContents(web_contents)
           ->AddInfoBar(std::move(infobar)))
    return false;

  if (is_webapk) {
    if (webapk_install_source == webapk::INSTALL_SOURCE_MENU) {
      raw_delegate->Accept();
      webapk::TrackInstallInfoBarShown(webapk::WEBAPK_INFOBAR_SHOWN_FROM_MENU);
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
    std::unique_ptr<SkBitmap> icon,
    const std::string& native_app_package,
    const std::string& referrer,
    int event_request_id) {
  auto infobar_delegate = base::WrapUnique(new AppBannerInfoBarDelegateAndroid(
      app_title, native_app_data, std::move(icon), native_app_package, referrer,
      event_request_id));
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
      TrackUserResponse(USER_RESPONSE_WEB_APP_IGNORED);
      if (is_webapk_)
        webapk::TrackInstallEvent(webapk::INFOBAR_IGNORED);
    }
  }

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

bool AppBannerInfoBarDelegateAndroid::AcceptWebApk(
    content::WebContents* web_contents) {
  if (IsInfoEmpty(shortcut_info_))
    return true;

  JNIEnv* env = base::android::AttachCurrentThread();
  // |webapk_package_name_| is set when the WebAPK has finished installing.
  // If the |webapk_package_name_| is empty, it means the "Add to Homescreen"
  // button is pressed, so request WebAPK installation. Otherwise, it means
  // the "Open" button is pressed, so open the installed WebAPK.
  if (!webapk_package_name_.empty()) {
    // Open the WebAPK.
    ScopedJavaLocalRef<jstring> java_webapk_package_name =
        base::android::ConvertUTF8ToJavaString(env, webapk_package_name_);
    Java_AppBannerInfoBarDelegateAndroid_openWebApk(env, java_delegate_,
                                                    java_webapk_package_name);
    webapk::TrackUserAction(webapk::USER_ACTION_INSTALLED_OPEN);
    SendBannerAccepted(web_contents, "web");
    return true;
  }

  // Check whether the WebAPK has been installed.
  std::string installed_webapk_package_name =
      ShortcutHelper::QueryWebApkPackage(web_contents->GetLastCommittedURL());
  if (installed_webapk_package_name.empty()) {
    // Request install the WebAPK.
    install_state_ = INSTALLING;
    TrackUserResponse(USER_RESPONSE_WEB_APP_ACCEPTED);
    webapk::TrackInstallSource(webapk_install_source_);
    AppBannerSettingsHelper::RecordBannerInstallEvent(
        web_contents, shortcut_info_->url.spec(), AppBannerSettingsHelper::WEB);

    Java_AppBannerInfoBarDelegateAndroid_setWebApkInstallingState(
        env, java_delegate_, true);
    UpdateInstallState(env, nullptr);
    WebApkInstaller::FinishCallback callback =
        base::Bind(&AppBannerInfoBarDelegateAndroid::OnWebApkInstallFinished,
                    weak_ptr_factory_.GetWeakPtr());
    ShortcutHelper::InstallWebApkWithSkBitmap(web_contents->GetBrowserContext(),
                                              *shortcut_info_,
                                              *icon_.get(), callback);
    SendBannerAccepted(web_contents, "web");

    // Prevent the infobar from disappearing, because the infobar will show
    // "Adding" during the installation process.
    return false;
  }

  // Bypass the installation since WebAPK is already installed.
  TrackUserResponse(USER_RESPONSE_WEB_APP_ACCEPTED);
  OnWebApkInstallFinished(true, installed_webapk_package_name);
  return false;
}

AppBannerInfoBarDelegateAndroid::AppBannerInfoBarDelegateAndroid(
    base::WeakPtr<AppBannerManager> weak_manager,
    const base::string16& app_title,
    std::unique_ptr<ShortcutInfo> shortcut_info,
    std::unique_ptr<SkBitmap> icon,
    int event_request_id,
    bool is_webapk,
    webapk::InstallSource webapk_install_source)
    : weak_manager_(weak_manager),
      app_title_(app_title),
      shortcut_info_(std::move(shortcut_info)),
      icon_(std::move(icon)),
      event_request_id_(event_request_id),
      has_user_interaction_(false),
      is_webapk_(is_webapk),
      install_state_(INSTALL_NOT_STARTED),
      webapk_install_source_(webapk_install_source),
      weak_ptr_factory_(this) {
  DCHECK(!IsInfoEmpty(shortcut_info_));
  CreateJavaDelegate();
}

AppBannerInfoBarDelegateAndroid::AppBannerInfoBarDelegateAndroid(
    const base::string16& app_title,
    const base::android::ScopedJavaGlobalRef<jobject>& native_app_data,
    std::unique_ptr<SkBitmap> icon,
    const std::string& native_app_package,
    const std::string& referrer,
    int event_request_id)
    : app_title_(app_title),
      native_app_data_(native_app_data),
      icon_(std::move(icon)),
      native_app_package_(native_app_package),
      referrer_(referrer),
      event_request_id_(event_request_id),
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

void AppBannerInfoBarDelegateAndroid::SendBannerAccepted(
    content::WebContents* web_contents,
    const std::string& platform) {
  web_contents->GetMainFrame()->Send(
      new ChromeViewMsg_AppBannerAccepted(
          web_contents->GetMainFrame()->GetRoutingID(),
          event_request_id_,
          platform));
}

infobars::InfoBarDelegate::InfoBarIdentifier
AppBannerInfoBarDelegateAndroid::GetIdentifier() const {
  return APP_BANNER_INFOBAR_DELEGATE_ANDROID;
}

gfx::Image AppBannerInfoBarDelegateAndroid::GetIcon() const {
  return gfx::Image::CreateFrom1xBitmap(*icon_.get());
}

void AppBannerInfoBarDelegateAndroid::InfoBarDismissed() {
  has_user_interaction_ = true;

  content::WebContents* web_contents =
      InfoBarService::WebContentsFromInfoBar(infobar());

  web_contents->GetMainFrame()->Send(
      new ChromeViewMsg_AppBannerDismissed(
          web_contents->GetMainFrame()->GetRoutingID(),
          event_request_id_));

  if (native_app_data_.is_null()) {
    if (is_webapk_)
      TrackWebApkInstallationDismissEvents(install_state_);
    TrackUserResponse(USER_RESPONSE_WEB_APP_DISMISSED);
    AppBannerSettingsHelper::RecordBannerDismissEvent(
        web_contents, shortcut_info_->url.spec(), AppBannerSettingsHelper::WEB);
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

  SendBannerAccepted(web_contents, "play");
  return was_opened;
}

bool AppBannerInfoBarDelegateAndroid::AcceptWebApp(
    content::WebContents* web_contents) {
  if (IsInfoEmpty(shortcut_info_))
    return true;
  TrackUserResponse(USER_RESPONSE_WEB_APP_ACCEPTED);

  AppBannerSettingsHelper::RecordBannerInstallEvent(
      web_contents, shortcut_info_->url.spec(), AppBannerSettingsHelper::WEB);

  if (weak_manager_) {
    const std::string& uid = base::GenerateGUID();
    ShortcutHelper::AddToLauncherWithSkBitmap(
        web_contents->GetBrowserContext(), *shortcut_info_, uid,
        *icon_.get(), weak_manager_->FetchWebappSplashScreenImageCallback(uid));
  }

  SendBannerAccepted(web_contents, "web");
  return true;
}

void AppBannerInfoBarDelegateAndroid::OnWebApkInstallFinished(
    bool success,
    const std::string& webapk_package_name) {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!success) {
    DVLOG(1) << "The WebAPK installation failed.";
    Java_AppBannerInfoBarDelegateAndroid_showWebApkInstallFailureToast(env);
    webapk::TrackInstallEvent(webapk::INSTALL_FAILED);
    if (infobar())
      infobar()->RemoveSelf();
    return;
  }

  webapk_package_name_ = webapk_package_name;
  ScopedJavaLocalRef<jstring> java_webapk_package_name =
      base::android::ConvertUTF8ToJavaString(env, webapk_package_name);
  Java_AppBannerInfoBarDelegateAndroid_setWebApkInstallingState(
      env, java_delegate_, false);
  Java_AppBannerInfoBarDelegateAndroid_setWebApkPackageName(
      env, java_delegate_, java_webapk_package_name);
  UpdateInstallState(env, nullptr);
  install_state_ = INSTALLED;
  webapk::TrackInstallEvent(webapk::INSTALL_COMPLETED);
}

void AppBannerInfoBarDelegateAndroid::TrackWebApkInstallationDismissEvents(
    InstallState install_state) {
  if (install_state == INSTALL_NOT_STARTED) {
    webapk::TrackInstallEvent(webapk::INFOBAR_DISMISSED_BEFORE_INSTALLATION);
  } else if (install_state == INSTALLING) {
    webapk::TrackInstallEvent(webapk::INFOBAR_DISMISSED_DURING_INSTALLATION);
  } else if (install_state == INSTALLED) {
    // When |install_state| is INSTALLED, the install Event will be recorded in
    // OnWebApkInstallFinished().
    webapk::TrackUserAction(webapk::USER_ACTION_INSTALLED_OPEN_DISMISS);
  }
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

bool RegisterAppBannerInfoBarDelegateAndroid(JNIEnv* env) {
 return RegisterNativesImpl(env);
}

}  // namespace banners
