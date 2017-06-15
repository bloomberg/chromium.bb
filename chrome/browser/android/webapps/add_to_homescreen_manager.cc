// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapps/add_to_homescreen_manager.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/android/banners/app_banner_infobar_delegate_android.h"
#include "chrome/browser/android/banners/app_banner_manager_android.h"
#include "chrome/browser/android/shortcut_helper.h"
#include "chrome/browser/android/webapk/chrome_webapk_host.h"
#include "chrome/browser/android/webapk/webapk_install_service.h"
#include "chrome/browser/android/webapk/webapk_metrics.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/installable/installable_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "jni/AddToHomescreenManager_jni.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/WebKit/public/platform/modules/installation/installation.mojom.h"
#include "ui/gfx/android/java_bitmap.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

jlong InitializeAndStart(JNIEnv* env,
                         const JavaParamRef<jobject>& obj,
                         const JavaParamRef<jobject>& java_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  AddToHomescreenManager* manager = new AddToHomescreenManager(env, obj);
  manager->Start(web_contents);
  return reinterpret_cast<intptr_t>(manager);
}

AddToHomescreenManager::AddToHomescreenManager(JNIEnv* env, jobject obj)
    : is_webapk_compatible_(false) {
  java_ref_.Reset(env, obj);
}

// static
bool AddToHomescreenManager::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void AddToHomescreenManager::Destroy(JNIEnv* env,
                                     const JavaParamRef<jobject>& obj) {
  delete this;
}

void AddToHomescreenManager::AddShortcut(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& j_user_title) {
  content::WebContents* web_contents = data_fetcher_->web_contents();
  if (!web_contents)
    return;

  base::string16 user_title =
      base::android::ConvertJavaStringToUTF16(env, j_user_title);
  if (!user_title.empty())
    data_fetcher_->shortcut_info().user_title = user_title;

  RecordAddToHomescreen();
  ShortcutHelper::AddToLauncherWithSkBitmap(web_contents,
                                            data_fetcher_->shortcut_info(),
                                            data_fetcher_->primary_icon());

  // Fire the appinstalled event.
  banners::AppBannerManagerAndroid* app_banner_manager =
      banners::AppBannerManagerAndroid::FromWebContents(web_contents);
  app_banner_manager->OnInstall();
}

void AddToHomescreenManager::Start(content::WebContents* web_contents) {
  bool check_webapk_compatible = false;
  if (ChromeWebApkHost::CanInstallWebApk() &&
      InstallableManager::IsContentSecure(web_contents)) {
    check_webapk_compatible = true;
  } else {
    ShowDialog();
  }

  data_fetcher_ = new AddToHomescreenDataFetcher(
      web_contents, ShortcutHelper::GetIdealHomescreenIconSizeInPx(),
      ShortcutHelper::GetMinimumHomescreenIconSizeInPx(),
      ShortcutHelper::GetIdealSplashImageSizeInPx(),
      ShortcutHelper::GetMinimumSplashImageSizeInPx(),
      ShortcutHelper::GetIdealBadgeIconSizeInPx(),
      check_webapk_compatible, this);
}

AddToHomescreenManager::~AddToHomescreenManager() {
  if (data_fetcher_) {
    data_fetcher_->set_weak_observer(nullptr);
    data_fetcher_ = nullptr;
  }
}

void AddToHomescreenManager::ShowDialog() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AddToHomescreenManager_showDialog(env, java_ref_);
}

void AddToHomescreenManager::RecordAddToHomescreen() {
  // Record that the shortcut has been added, so no banners will be shown
  // for this app.
  content::WebContents* web_contents = data_fetcher_->web_contents();
  if (!web_contents)
    return;

  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents, web_contents->GetURL(),
      data_fetcher_->shortcut_info().url.spec(),
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_ADD_TO_HOMESCREEN,
      base::Time::Now());
}

void AddToHomescreenManager::OnDidDetermineWebApkCompatibility(
    bool is_webapk_compatible) {
  is_webapk_compatible_ = is_webapk_compatible;
  if (!is_webapk_compatible)
    ShowDialog();
}

void AddToHomescreenManager::OnUserTitleAvailable(
    const base::string16& user_title) {
  if (is_webapk_compatible_)
    return;
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_user_title =
      base::android::ConvertUTF16ToJavaString(env, user_title);
  Java_AddToHomescreenManager_onUserTitleAvailable(env,
                                                   java_ref_,
                                                   j_user_title);
}

void AddToHomescreenManager::OnDataAvailable(const ShortcutInfo& info,
                                             const SkBitmap& primary_icon,
                                             const SkBitmap& badge_icon) {
  if (is_webapk_compatible_) {
    WebApkInstallService* install_service =
        WebApkInstallService::Get(
            data_fetcher_->web_contents()->GetBrowserContext());
    if (install_service->IsInstallInProgress(info.manifest_url))
      ShortcutHelper::ShowWebApkInstallInProgressToast();
    else
      CreateInfoBarForWebApk(info, primary_icon, badge_icon);

    JNIEnv* env = base::android::AttachCurrentThread();
    Java_AddToHomescreenManager_onFinished(env, java_ref_);
    return;
  }

  ScopedJavaLocalRef<jobject> java_bitmap;
  if (!primary_icon.drawsNothing())
    java_bitmap = gfx::ConvertToJavaBitmap(&primary_icon);

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AddToHomescreenManager_onReadyToAdd(env, java_ref_, java_bitmap);
}

void AddToHomescreenManager::CreateInfoBarForWebApk(
    const ShortcutInfo& info,
    const SkBitmap& primary_icon,
    const SkBitmap& badge_icon) {
  content::WebContents* web_contents = data_fetcher_->web_contents();
  banners::AppBannerManagerAndroid* app_banner_manager =
      banners::AppBannerManagerAndroid::FromWebContents(web_contents);
  banners::AppBannerInfoBarDelegateAndroid::Create(
      web_contents, app_banner_manager->GetWeakPtr(), info.user_title,
      base::MakeUnique<ShortcutInfo>(info), primary_icon, badge_icon,
      -1 /* event_request_id */, true /* is_webapk */,
      webapk::INSTALL_SOURCE_MENU);
}

SkBitmap AddToHomescreenManager::FinalizeLauncherIconInBackground(
    const SkBitmap& bitmap,
    const GURL& url,
    bool* is_generated) {
  base::ThreadRestrictions::AssertIOAllowed();

  return ShortcutHelper::FinalizeLauncherIconInBackground(bitmap, url,
                                                          is_generated);
}
