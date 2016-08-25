// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/instantapps/instant_apps_infobar_delegate.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/android/infobars/instant_apps_infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "content/public/browser/web_contents.h"
#include "jni/InstantAppsInfoBarDelegate_jni.h"

InstantAppsInfoBarDelegate::~InstantAppsInfoBarDelegate() {}

// static
void InstantAppsInfoBarDelegate::Create(InfoBarService* infobar_service,
                                        jobject jdata) {
  infobar_service->AddInfoBar(base::MakeUnique<InstantAppsInfoBar>(
      std::unique_ptr<InstantAppsInfoBarDelegate>(
          new InstantAppsInfoBarDelegate(jdata))));
}

InstantAppsInfoBarDelegate::InstantAppsInfoBarDelegate(jobject jdata) {
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
  Java_InstantAppsInfoBarDelegate_openInstantApp(env, java_delegate_.obj());
  return true;
}

bool InstantAppsInfoBarDelegate::EqualsDelegate(
    infobars::InfoBarDelegate* delegate) const {
  return delegate && delegate->GetIdentifier() == GetIdentifier();
}

void Launch(JNIEnv* env,
            const base::android::JavaParamRef<jclass>& clazz,
            const base::android::JavaParamRef<jobject>& jweb_contents,
            const base::android::JavaParamRef<jobject>& jdata) {
  auto web_contents = content::WebContents::FromJavaWebContents(jweb_contents);
  InstantAppsInfoBarDelegate::Create(
      InfoBarService::FromWebContents(web_contents),
      jdata);
}

bool RegisterInstantAppsInfoBarDelegate(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
