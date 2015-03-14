// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/banners/app_banner_manager_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/command_line.h"
#include "chrome/browser/android/banners/app_banner_data_fetcher_android.h"
#include "chrome/browser/banners/app_banner_metrics.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/frame_navigate_params.h"
#include "jni/AppBannerManager_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;

namespace {
const char kBannerTag[] = "google-play-id";
}  // anonymous namespace

namespace banners {

AppBannerManagerAndroid::AppBannerManagerAndroid(JNIEnv* env,
                                                 jobject obj,
                                                 int icon_size)
    : AppBannerManager(icon_size),
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

bool AppBannerManagerAndroid::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AppBannerManagerAndroid, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_DidRetrieveMetaTagContent,
                        OnDidRetrieveMetaTagContent)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool AppBannerManagerAndroid::OnInvalidManifest(AppBannerDataFetcher* fetcher) {
  DCHECK(data_fetcher() == fetcher);
  if (web_contents()->IsBeingDestroyed() || !IsEnabledForNativeApps()) {
    return false;
  }

  Send(new ChromeViewMsg_RetrieveMetaTagContent(routing_id(),
                                                fetcher->validated_url(),
                                                kBannerTag));
  return true;
}

AppBannerDataFetcher* AppBannerManagerAndroid::CreateAppBannerDataFetcher(
    base::WeakPtr<Delegate> weak_delegate,
    const int ideal_icon_size) {
  return new AppBannerDataFetcherAndroid(web_contents(), weak_delegate,
                                         ideal_icon_size);
}

void AppBannerManagerAndroid::OnDidRetrieveMetaTagContent(
    bool success,
    const std::string& tag_name,
    const std::string& tag_content,
    const GURL& expected_url) {
  DCHECK(web_contents());
  if (!success
      || tag_name != kBannerTag
      || !data_fetcher()
      || data_fetcher()->validated_url() != expected_url
      || tag_content.size() >= chrome::kMaxMetaTagAttributeLength) {
    return;
  }

  banners::TrackDisplayEvent(DISPLAY_EVENT_BANNER_REQUESTED);

  // Send the info to the Java side to get info about the app.
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jobj = weak_java_banner_view_manager_.get(env);
  if (jobj.is_null())
    return;

  ScopedJavaLocalRef<jstring> jurl(
      ConvertUTF8ToJavaString(env, expected_url.spec()));
  ScopedJavaLocalRef<jstring> jpackage(
      ConvertUTF8ToJavaString(env, tag_content));
  Java_AppBannerManager_fetchAppDetails(
      env, jobj.obj(), jurl.obj(), jpackage.obj(), ideal_icon_size());
}

bool AppBannerManagerAndroid::OnAppDetailsRetrieved(JNIEnv* env,
                                                    jobject obj,
                                                    jobject japp_data,
                                                    jstring japp_title,
                                                    jstring japp_package,
                                                    jstring jicon_url) {
  if (!data_fetcher() || !web_contents()
      || data_fetcher()->validated_url() != web_contents()->GetURL()) {
    return false;
  }

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
bool AppBannerManagerAndroid::IsEnabledForNativeApps() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableAppInstallAlerts);
}

// static
bool AppBannerManagerAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

jlong Init(JNIEnv* env, jobject obj, jint icon_size) {
  AppBannerManagerAndroid* manager =
      new AppBannerManagerAndroid(env, obj, icon_size);
  return reinterpret_cast<intptr_t>(manager);
}

void SetTimeDeltaForTesting(JNIEnv* env, jclass clazz, jint days) {
  AppBannerDataFetcher::SetTimeDeltaForTesting(days);
}

void DisableSecureSchemeCheckForTesting(JNIEnv* env, jclass clazz) {
  AppBannerManager::DisableSecureSchemeCheckForTesting();
}

jboolean IsEnabled(JNIEnv* env, jclass clazz) {
  return AppBannerManager::IsEnabled();
}

}  // namespace banners
