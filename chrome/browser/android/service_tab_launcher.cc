// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/service_tab_launcher.h"

#include "base/android/jni_string.h"
#include "base/callback.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/page_navigator.h"
#include "jni/ServiceTabLauncher_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::GetApplicationContext;

namespace chrome {
namespace android {

// static
ServiceTabLauncher* ServiceTabLauncher::GetInstance() {
  return Singleton<ServiceTabLauncher>::get();
}

ServiceTabLauncher::ServiceTabLauncher() {
  java_object_.Reset(
      Java_ServiceTabLauncher_create(
          AttachCurrentThread(),
          reinterpret_cast<intptr_t>(this),
          GetApplicationContext()));
}

ServiceTabLauncher::~ServiceTabLauncher() {}

void ServiceTabLauncher::LaunchTab(
    content::BrowserContext* browser_context,
    const content::OpenURLParams& params,
    const base::Callback<void(content::WebContents*)>& callback) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> url = ConvertUTF8ToJavaString(
      env, params.url.spec());

  Java_ServiceTabLauncher_launchTab(env,
                                    java_object_.obj(),
                                    url.obj(),
                                    params.disposition,
                                    browser_context->IsOffTheRecord());

  // TODO(peter): We need to wait for the Android Activity to reply to the
  // launch intent with the ID of the launched Web Contents, so that the Java
  // side can invoke a method on the native side with the request id and the
  // WebContents enabling us to invoke |callback|. See https://crbug.com/454809.
}

bool ServiceTabLauncher::RegisterServiceTabLauncher(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android
}  // namespace chrome
