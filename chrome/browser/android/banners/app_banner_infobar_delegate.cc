// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/banners/app_banner_infobar_delegate.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/location.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/worker_pool.h"
#include "chrome/browser/android/shortcut_helper.h"
#include "chrome/browser/android/shortcut_info.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/banners/app_banner_data_fetcher.h"
#include "chrome/browser/banners/app_banner_metrics.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/android/infobars/app_banner_infobar.h"
#include "chrome/grit/generated_resources.h"
#include "components/rappor/rappor_utils.h"
#include "content/public/common/manifest.h"
#include "jni/AppBannerInfoBarDelegate_jni.h"
#include "ui/gfx/android/java_bitmap.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;

namespace banners {

AppBannerInfoBarDelegate::AppBannerInfoBarDelegate(
    const base::string16& app_title,
    SkBitmap* app_icon,
    const content::Manifest& web_app_data)
    : app_title_(app_title),
      app_icon_(app_icon),
      web_app_data_(web_app_data) {
  DCHECK(!web_app_data.IsEmpty());
  CreateJavaDelegate();
}

AppBannerInfoBarDelegate::AppBannerInfoBarDelegate(
    const base::string16& app_title,
    SkBitmap* app_icon,
    const base::android::ScopedJavaGlobalRef<jobject>& native_app_data,
    const std::string& native_app_package)
    : app_title_(app_title),
      app_icon_(app_icon),
      native_app_data_(native_app_data),
      native_app_package_(native_app_package) {
  DCHECK(!native_app_data_.is_null());
  CreateJavaDelegate();
}

AppBannerInfoBarDelegate::~AppBannerInfoBarDelegate() {
  TrackDismissEvent(DISMISS_EVENT_DISMISSED);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AppBannerInfoBarDelegate_destroy(env,
                                        java_delegate_.obj());
  java_delegate_.Reset();
}

void AppBannerInfoBarDelegate::UpdateInstallState(JNIEnv* env, jobject obj) {
  if (native_app_data_.is_null())
    return;

  int newState = Java_AppBannerInfoBarDelegate_determineInstallState(
      env,
      java_delegate_.obj(),
      native_app_data_.obj());
  static_cast<AppBannerInfoBar*>(infobar())->OnInstallStateChanged(newState);
}

void AppBannerInfoBarDelegate::OnInstallIntentReturned(
    JNIEnv* env,
    jobject obj,
    jboolean jis_installing) {
  if (!infobar())
    return;

  content::WebContents* web_contents =
      InfoBarService::WebContentsFromInfoBar(infobar());
  if (!web_contents)
    return;

  if (jis_installing) {
    AppBannerSettingsHelper::RecordBannerEvent(
        web_contents,
        web_contents->GetURL(),
        native_app_package_,
        AppBannerSettingsHelper::APP_BANNER_EVENT_DID_ADD_TO_HOMESCREEN,
        AppBannerDataFetcher::GetCurrentTime());

    TrackInstallEvent(INSTALL_EVENT_NATIVE_APP_INSTALL_STARTED);
    rappor::SampleDomainAndRegistryFromGURL(g_browser_process->rappor_service(),
                                            "AppBanner.NativeApp.Installed",
                                            web_contents->GetURL());
  }

  UpdateInstallState(env, obj);
}

void AppBannerInfoBarDelegate::OnInstallFinished(JNIEnv* env,
                                                 jobject obj,
                                                 jboolean success) {
  if (!infobar())
    return;

  if (success) {
    TrackInstallEvent(INSTALL_EVENT_NATIVE_APP_INSTALL_COMPLETED);
    UpdateInstallState(env, obj);
  } else if (infobar()->owner()) {
    TrackDismissEvent(DISMISS_EVENT_INSTALL_TIMEOUT);
    infobar()->owner()->RemoveInfoBar(infobar());
  }
}

void AppBannerInfoBarDelegate::CreateJavaDelegate() {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_delegate_.Reset(Java_AppBannerInfoBarDelegate_create(
      env,
      reinterpret_cast<intptr_t>(this)));
}

gfx::Image AppBannerInfoBarDelegate::GetIcon() const {
  return gfx::Image::CreateFrom1xBitmap(*app_icon_.get());
}

void AppBannerInfoBarDelegate::InfoBarDismissed() {
  content::WebContents* web_contents =
      InfoBarService::WebContentsFromInfoBar(infobar());
  if (!web_contents)
    return;

  TrackDismissEvent(DISMISS_EVENT_CLOSE_BUTTON);

  if (!native_app_data_.is_null()) {
    AppBannerSettingsHelper::RecordBannerEvent(
        web_contents, web_contents->GetURL(),
        native_app_package_,
        AppBannerSettingsHelper::APP_BANNER_EVENT_DID_BLOCK,
        AppBannerDataFetcher::GetCurrentTime());

    rappor::SampleDomainAndRegistryFromGURL(g_browser_process->rappor_service(),
                                            "AppBanner.NativeApp.Dismissed",
                                            web_contents->GetURL());
  } else if (!web_app_data_.IsEmpty()) {
    AppBannerSettingsHelper::RecordBannerEvent(
        web_contents, web_contents->GetURL(),
        web_app_data_.start_url.spec(),
        AppBannerSettingsHelper::APP_BANNER_EVENT_DID_BLOCK,
        AppBannerDataFetcher::GetCurrentTime());

    rappor::SampleDomainAndRegistryFromGURL(g_browser_process->rappor_service(),
                                            "AppBanner.WebApp.Dismissed",
                                            web_contents->GetURL());
  }
}

base::string16 AppBannerInfoBarDelegate::GetMessageText() const {
  return app_title_;
}

int AppBannerInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

bool AppBannerInfoBarDelegate::Accept() {
  content::WebContents* web_contents =
      InfoBarService::WebContentsFromInfoBar(infobar());
  if (!web_contents) {
    TrackDismissEvent(DISMISS_EVENT_ERROR);
    return true;
  }

  if (!native_app_data_.is_null()) {
    JNIEnv* env = base::android::AttachCurrentThread();

    TabAndroid* tab = TabAndroid::FromWebContents(web_contents);
    if (tab == nullptr) {
      TrackDismissEvent(DISMISS_EVENT_ERROR);
      return true;
    }

    bool was_opened = Java_AppBannerInfoBarDelegate_installOrOpenNativeApp(
        env,
        java_delegate_.obj(),
        tab->GetJavaObject().obj(),
        native_app_data_.obj());

    if (was_opened) {
      TrackDismissEvent(DISMISS_EVENT_APP_OPEN);
    } else {
      TrackInstallEvent(INSTALL_EVENT_NATIVE_APP_INSTALL_TRIGGERED);
    }
    return was_opened;
  } else if (!web_app_data_.IsEmpty()) {
    AppBannerSettingsHelper::RecordBannerEvent(
        web_contents, web_contents->GetURL(),
        web_app_data_.start_url.spec(),
        AppBannerSettingsHelper::APP_BANNER_EVENT_DID_ADD_TO_HOMESCREEN,
        AppBannerDataFetcher::GetCurrentTime());

    ShortcutInfo info;
    info.UpdateFromManifest(web_app_data_);
    base::WorkerPool::PostTask(
        FROM_HERE,
        base::Bind(&ShortcutHelper::AddShortcutInBackgroundWithSkBitmap,
                   info,
                   *app_icon_.get(),
                   false),
        true);

    TrackInstallEvent(INSTALL_EVENT_WEB_APP_INSTALLED);
    rappor::SampleDomainAndRegistryFromGURL(g_browser_process->rappor_service(),
                                            "AppBanner.WebApp.Installed",
                                            web_contents->GetURL());
    return true;
  }

  return true;
}

bool AppBannerInfoBarDelegate::LinkClicked(WindowOpenDisposition disposition) {
  if (native_app_data_.is_null())
    return false;

  // Try to show the details for the native app.
  JNIEnv* env = base::android::AttachCurrentThread();

  content::WebContents* web_contents =
      InfoBarService::WebContentsFromInfoBar(infobar());
  TabAndroid* tab = web_contents ? TabAndroid::FromWebContents(web_contents)
                                 : nullptr;
  if (tab == nullptr) {
    TrackDismissEvent(DISMISS_EVENT_ERROR);
    return true;
  }

  Java_AppBannerInfoBarDelegate_showAppDetails(env,
                                               java_delegate_.obj(),
                                               tab->GetJavaObject().obj(),
                                               native_app_data_.obj());

  TrackDismissEvent(DISMISS_EVENT_BANNER_CLICK);
  return true;
}

bool RegisterAppBannerInfoBarDelegate(JNIEnv* env) {
 return RegisterNativesImpl(env);
}

}  // namespace banners
