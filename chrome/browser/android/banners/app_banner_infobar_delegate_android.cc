// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/banners/app_banner_infobar_delegate_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/guid.h"
#include "base/location.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/shortcut_helper.h"
#include "chrome/browser/android/shortcut_info.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/banners/app_banner_data_fetcher.h"
#include "chrome/browser/banners/app_banner_metrics.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/android/infobars/app_banner_infobar_android.h"
#include "chrome/common/render_messages.h"
#include "chrome/grit/generated_resources.h"
#include "components/rappor/rappor_utils.h"
#include "content/public/browser/browser_thread.h"
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

namespace banners {

AppBannerInfoBarDelegateAndroid::AppBannerInfoBarDelegateAndroid(
    int event_request_id,
    scoped_refptr<AppBannerDataFetcherAndroid> data_fetcher,
    const base::string16& app_title,
    SkBitmap* app_icon,
    const content::Manifest& web_app_data)
    : data_fetcher_(data_fetcher),
      app_title_(app_title),
      app_icon_(app_icon),
      event_request_id_(event_request_id),
      web_app_data_(web_app_data),
      has_user_interaction_(false) {
  DCHECK(!web_app_data.IsEmpty());
  CreateJavaDelegate();
}

AppBannerInfoBarDelegateAndroid::AppBannerInfoBarDelegateAndroid(
    int event_request_id,
    const base::string16& app_title,
    SkBitmap* app_icon,
    const base::android::ScopedJavaGlobalRef<jobject>& native_app_data,
    const std::string& native_app_package,
    const std::string& referrer)
    : app_title_(app_title),
      app_icon_(app_icon),
      event_request_id_(event_request_id),
      native_app_data_(native_app_data),
      native_app_package_(native_app_package),
      referrer_(referrer),
      has_user_interaction_(false) {
  DCHECK(!native_app_data_.is_null());
  CreateJavaDelegate();
}

AppBannerInfoBarDelegateAndroid::~AppBannerInfoBarDelegateAndroid() {
  if (!has_user_interaction_) {
    if (!native_app_data_.is_null())
      TrackUserResponse(USER_RESPONSE_NATIVE_APP_IGNORED);
    else if (!web_app_data_.IsEmpty())
      TrackUserResponse(USER_RESPONSE_WEB_APP_IGNORED);
  }

  TrackDismissEvent(DISMISS_EVENT_DISMISSED);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AppBannerInfoBarDelegateAndroid_destroy(env,
                                        java_delegate_.obj());
  java_delegate_.Reset();
}

void AppBannerInfoBarDelegateAndroid::UpdateInstallState(JNIEnv* env,
                                                         jobject obj) {
  if (native_app_data_.is_null())
    return;

  int newState = Java_AppBannerInfoBarDelegateAndroid_determineInstallState(
      env,
      java_delegate_.obj(),
      native_app_data_.obj());
  static_cast<AppBannerInfoBarAndroid*>(infobar())
      ->OnInstallStateChanged(newState);
}

void AppBannerInfoBarDelegateAndroid::OnInstallIntentReturned(
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

void AppBannerInfoBarDelegateAndroid::OnInstallFinished(JNIEnv* env,
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

void AppBannerInfoBarDelegateAndroid::CreateJavaDelegate() {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_delegate_.Reset(Java_AppBannerInfoBarDelegateAndroid_create(
      env,
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

gfx::Image AppBannerInfoBarDelegateAndroid::GetIcon() const {
  return gfx::Image::CreateFrom1xBitmap(*app_icon_.get());
}

void AppBannerInfoBarDelegateAndroid::InfoBarDismissed() {
  has_user_interaction_ = true;

  content::WebContents* web_contents =
      InfoBarService::WebContentsFromInfoBar(infobar());
  if (!web_contents)
    return;

  web_contents->GetMainFrame()->Send(
      new ChromeViewMsg_AppBannerDismissed(
          web_contents->GetMainFrame()->GetRoutingID(),
          event_request_id_));

  if (!native_app_data_.is_null()) {
    TrackUserResponse(USER_RESPONSE_NATIVE_APP_DISMISSED);
    AppBannerSettingsHelper::RecordBannerDismissEvent(
        web_contents, native_app_package_, AppBannerSettingsHelper::NATIVE);
  } else if (!web_app_data_.IsEmpty()) {
    TrackUserResponse(USER_RESPONSE_WEB_APP_DISMISSED);
    AppBannerSettingsHelper::RecordBannerDismissEvent(
        web_contents, web_app_data_.start_url.spec(),
        AppBannerSettingsHelper::WEB);
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

  if (!native_app_data_.is_null()) {
    TrackUserResponse(USER_RESPONSE_NATIVE_APP_ACCEPTED);
    JNIEnv* env = base::android::AttachCurrentThread();

    TabAndroid* tab = TabAndroid::FromWebContents(web_contents);
    if (tab == nullptr) {
      TrackDismissEvent(DISMISS_EVENT_ERROR);
      return true;
    }
    ScopedJavaLocalRef<jstring> jreferrer(
        ConvertUTF8ToJavaString(env, referrer_));

    bool was_opened = Java_AppBannerInfoBarDelegateAndroid_installOrOpenNativeApp(
        env,
        java_delegate_.obj(),
        tab->GetJavaObject().obj(),
        native_app_data_.obj(),
        jreferrer.obj());

    if (was_opened) {
      TrackDismissEvent(DISMISS_EVENT_APP_OPEN);
    } else {
      TrackInstallEvent(INSTALL_EVENT_NATIVE_APP_INSTALL_TRIGGERED);
    }
    SendBannerAccepted(web_contents, "play");
    return was_opened;
  } else if (!web_app_data_.IsEmpty()) {
    TrackUserResponse(USER_RESPONSE_WEB_APP_ACCEPTED);

    AppBannerSettingsHelper::RecordBannerInstallEvent(
        web_contents, web_app_data_.start_url.spec(),
        AppBannerSettingsHelper::WEB);

    ShortcutInfo info(GURL::EmptyGURL());
    info.UpdateFromManifest(web_app_data_);
    info.UpdateSource(ShortcutInfo::SOURCE_APP_BANNER);

    const std::string& uid = base::GenerateGUID();
    content::BrowserThread::PostTask(
        content::BrowserThread::IO,
        FROM_HERE,
        base::Bind(&ShortcutHelper::AddShortcutInBackgroundWithSkBitmap,
                   info,
                   uid,
                   *app_icon_.get()));
    data_fetcher_->FetchWebappSplashScreenImage(uid);

    SendBannerAccepted(web_contents, "web");
    return true;
  }

  return true;
}

bool AppBannerInfoBarDelegateAndroid::LinkClicked(
    WindowOpenDisposition disposition) {
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

  Java_AppBannerInfoBarDelegateAndroid_showAppDetails(env,
                                               java_delegate_.obj(),
                                               tab->GetJavaObject().obj(),
                                               native_app_data_.obj());

  TrackDismissEvent(DISMISS_EVENT_BANNER_CLICK);
  return true;
}

bool RegisterAppBannerInfoBarDelegateAndroid(JNIEnv* env) {
 return RegisterNativesImpl(env);
}

}  // namespace banners
