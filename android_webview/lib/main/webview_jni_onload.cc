// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/lib/main/webview_jni_onload.h"

#include "android_webview/common/aw_version_info_values.h"
#include "android_webview/lib/main/aw_main_delegate.h"
#include "android_webview/native/android_webview_jni_registrar.h"
#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "base/bind.h"
#include "components/web_contents_delegate_android/component_jni_registrar.h"
#include "content/public/app/content_jni_onload.h"
#include "content/public/app/content_main.h"
#include "url/url_util.h"

namespace android_webview {

namespace {

static base::android::RegistrationMethod
    kWebViewDependencyRegisteredMethods[] = {
    { "WebContentsDelegateAndroid",
        web_contents_delegate_android::RegisterWebContentsDelegateAndroidJni },
};

}  // namespace

bool OnJNIOnLoadRegisterJNI(JNIEnv* env) {
  if (!content::android::OnJNIOnLoadRegisterJNI(env))
    return false;

  // Register JNI for components we depend on.
  if (!RegisterNativeMethods(
          env,
          kWebViewDependencyRegisteredMethods,
          arraysize(kWebViewDependencyRegisteredMethods)) ||
      !android_webview::RegisterJni(env)) {
    return false;
  }
  return true;
}

bool OnJNIOnLoadInit() {
  if (!content::android::OnJNIOnLoadInit())
    return false;

  base::android::SetVersionNumber(PRODUCT_VERSION);
  content::SetContentMainDelegate(new android_webview::AwMainDelegate());

  // Initialize url lib here while we are still single-threaded, in case we use
  // CookieManager before initializing Chromium (which would normally have done
  // this). It's safe to call this multiple times.
  url::Initialize();
  return true;
}

}  // android_webview
