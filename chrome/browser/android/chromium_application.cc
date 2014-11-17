// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/chromium_application.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/common/chrome_content_client.h"
#include "content/public/browser/web_contents.h"
#include "jni/ChromiumApplication_jni.h"

using base::android::ConvertUTF8ToJavaString;

static jstring GetBrowserUserAgent(JNIEnv* env, jclass clazz) {
  return ConvertUTF8ToJavaString(env, GetUserAgent()).Release();
}

namespace chrome {
namespace android {

// static
bool ChromiumApplication::RegisterBindings(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void ChromiumApplication::OpenProtectedContentSettings() {
  Java_ChromiumApplication_openProtectedContentSettings(
      base::android::AttachCurrentThread(),
      base::android::GetApplicationContext());
}

void ChromiumApplication::ShowAutofillSettings() {
  Java_ChromiumApplication_showAutofillSettings(
      base::android::AttachCurrentThread(),
      base::android::GetApplicationContext());
}

void ChromiumApplication::ShowPasswordSettings() {
  Java_ChromiumApplication_showPasswordSettings(
      base::android::AttachCurrentThread(),
      base::android::GetApplicationContext());
}

void ChromiumApplication::OpenClearBrowsingData(
    content::WebContents* web_contents) {
  TabAndroid* tab = TabAndroid::FromWebContents(web_contents);
  DCHECK(tab);
  Java_ChromiumApplication_openClearBrowsingData(
      base::android::AttachCurrentThread(),
      base::android::GetApplicationContext(),
      tab->GetJavaObject().obj());
}

bool ChromiumApplication::AreParentalControlsEnabled() {
  return Java_ChromiumApplication_areParentalControlsEnabled(
      base::android::AttachCurrentThread(),
      base::android::GetApplicationContext());
}

}  // namespace android
}  // namespace chrome
