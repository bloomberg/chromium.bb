// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/instantapps/instant_apps_infobar_delegate.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/android/instantapps/instant_apps_settings.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/android/infobars/instant_apps_infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "content/public/browser/web_contents.h"
#include "jni/InstantAppsInfoBarDelegate_jni.h"

InstantAppsInfoBarDelegate::~InstantAppsInfoBarDelegate() {}

// static
void InstantAppsInfoBarDelegate::Create(InfoBarService* infobar_service,
                                        const jobject jdata,
                                        const std::string& url) {
  infobar_service->AddInfoBar(base::MakeUnique<InstantAppsInfoBar>(
      std::unique_ptr<InstantAppsInfoBarDelegate>(
          new InstantAppsInfoBarDelegate(jdata, url))));
}

InstantAppsInfoBarDelegate::InstantAppsInfoBarDelegate(const jobject jdata,
                                                       const std::string& url)
    : url_(url) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_delegate_.Reset(Java_InstantAppsInfoBarDelegate_create(env));
  data_.Reset(env, jdata);
}

infobars::InfoBarDelegate::InfoBarIdentifier
InstantAppsInfoBarDelegate::GetIdentifier() const {
  return INSTANT_APPS_INFOBAR_DELEGATE_ANDROID;
}

base::string16 InstantAppsInfoBarDelegate::GetMessageText() const {
  // Message is set in InstantAppInfobar.java
  return base::string16();
}

bool InstantAppsInfoBarDelegate::Accept() {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::RecordAction(base::UserMetricsAction(
      "Android.InstantApps.BannerOpen"));
  Java_InstantAppsInfoBarDelegate_openInstantApp(env,
                                                 java_delegate_.obj(),
                                                 data_.obj());
  return true;
}

bool InstantAppsInfoBarDelegate::EqualsDelegate(
    infobars::InfoBarDelegate* delegate) const {
  return delegate && delegate->GetIdentifier() == GetIdentifier();
}


void InstantAppsInfoBarDelegate::InfoBarDismissed() {
  content::WebContents* web_contents =
      InfoBarService::WebContentsFromInfoBar(infobar());
  InstantAppsSettings::RecordInfoBarDismissEvent(web_contents, url_);
  base::RecordAction(base::UserMetricsAction(
      "Android.InstantApps.BannerDismissed"));
}

void Launch(JNIEnv* env,
            const base::android::JavaParamRef<jclass>& clazz,
            const base::android::JavaParamRef<jobject>& jweb_contents,
            const base::android::JavaParamRef<jobject>& jdata,
            const base::android::JavaParamRef<jstring>& jurl) {
  auto* web_contents = content::WebContents::FromJavaWebContents(jweb_contents);
  std::string url(base::android::ConvertJavaStringToUTF8(env, jurl));
  InstantAppsInfoBarDelegate::Create(
      InfoBarService::FromWebContents(web_contents),
      jdata,
      url);
  InstantAppsSettings::RecordInfoBarShowEvent(web_contents, url);
  base::RecordAction(base::UserMetricsAction(
      "Android.InstantApps.BannerShown"));
}

bool RegisterInstantAppsInfoBarDelegate(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
