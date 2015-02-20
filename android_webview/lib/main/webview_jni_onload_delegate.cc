// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/lib/main/webview_jni_onload_delegate.h"

#include "android_webview/lib/main/aw_main_delegate.h"
#include "android_webview/native/android_webview_jni_registrar.h"
#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "components/navigation_interception/component_jni_registrar.h"
#include "components/web_contents_delegate_android/component_jni_registrar.h"
#include "content/public/app/content_jni_onload.h"
#include "content/public/app/content_main.h"
#include "url/url_util.h"

namespace android_webview {

namespace {

static base::android::RegistrationMethod
    kWebViewDependencyRegisteredMethods[] = {
    { "NavigationInterception",
        navigation_interception::RegisterNavigationInterceptionJni },
    { "WebContentsDelegateAndroid",
        web_contents_delegate_android::RegisterWebContentsDelegateAndroidJni },
};

}  // namespace

bool WebViewJNIOnLoadDelegate::RegisterJNI(JNIEnv* env) {
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

bool WebViewJNIOnLoadDelegate::Init() {
  content::SetContentMainDelegate(new android_webview::AwMainDelegate());

  // Initialize url lib here while we are still single-threaded, in case we use
  // CookieManager before initializing Chromium (which would normally have done
  // this). It's safe to call this multiple times.
  url::Initialize();
  return true;
}

bool OnJNIOnLoad(JavaVM* vm) {
  WebViewJNIOnLoadDelegate delegate;
  return content::android::OnJNIOnLoad(vm, &delegate);
}

}  // android_webview
