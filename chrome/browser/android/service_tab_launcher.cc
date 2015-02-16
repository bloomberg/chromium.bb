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
      Java_ServiceTabLauncher_getInstance(AttachCurrentThread(),
                                          GetApplicationContext()));
}

ServiceTabLauncher::~ServiceTabLauncher() {}

void ServiceTabLauncher::LaunchTab(
    content::BrowserContext* browser_context,
    const content::OpenURLParams& params,
    const base::Callback<void(content::WebContents*)>& callback) {
  if (!java_object_.obj()) {
    LOG(ERROR) << "No ServiceTabLauncher is available to launch a new tab.";
    callback.Run(nullptr);
    return;
  }

  WindowOpenDisposition disposition = params.disposition;
  if (disposition != NEW_WINDOW && disposition != NEW_POPUP &&
      disposition != NEW_FOREGROUND_TAB && disposition != NEW_BACKGROUND_TAB) {
    // ServiceTabLauncher can currently only launch new tabs.
    NOTIMPLEMENTED();
    return;
  }

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> url = ConvertUTF8ToJavaString(
      env, params.url.spec());
  ScopedJavaLocalRef<jstring> referrer_url =
      ConvertUTF8ToJavaString(env, params.referrer.url.spec());
  ScopedJavaLocalRef<jstring> headers = ConvertUTF8ToJavaString(
      env, params.extra_headers);

  ScopedJavaLocalRef<jbyteArray> post_data;

  Java_ServiceTabLauncher_launchTab(env,
                                    java_object_.obj(),
                                    GetApplicationContext(),
                                    0 /* request_id */,
                                    browser_context->IsOffTheRecord(),
                                    url.obj(),
                                    disposition,
                                    referrer_url.obj(),
                                    params.referrer.policy,
                                    headers.obj(),
                                    post_data.obj());

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
