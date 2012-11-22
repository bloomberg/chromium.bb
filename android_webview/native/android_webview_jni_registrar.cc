// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/android_webview_jni_registrar.h"

#include "android_webview/native/android_protocol_handler.h"
#include "android_webview/native/aw_contents.h"
#include "android_webview/native/aw_contents_io_thread_client_impl.h"
#include "android_webview/native/aw_http_auth_handler.h"
#include "android_webview/native/aw_resource.h"
#include "android_webview/native/aw_web_contents_delegate.h"
#include "android_webview/native/cookie_manager.h"
#include "android_webview/native/input_stream_impl.h"
#include "android_webview/native/intercepted_request_data_impl.h"
#include "android_webview/native/js_result_handler.h"
#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "content/components/navigation_interception/component_jni_registrar.h"
#include "content/components/web_contents_delegate_android/component_jni_registrar.h"

namespace android_webview {

static base::android::RegistrationMethod kWebViewRegisteredMethods[] = {
  // Register JNI for components we depend on.
  { "navigation_interception", content::RegisterNavigationInterceptionJni },
  { "web_contents_delegate_android",
      content::RegisterWebContentsDelegateAndroidJni },
  // Register JNI for android_webview classes.
  { "AndroidProtocolHandler", RegisterAndroidProtocolHandler },
  { "AwContents", RegisterAwContents },
  { "AwContentsIoThreadClientImpl", RegisterAwContentsIoThreadClientImpl},
  { "AwHttpAuthHandler", RegisterAwHttpAuthHandler },
  { "AwResource", AwResource::RegisterAwResource },
  { "AwWebContentsDelegate", RegisterAwWebContentsDelegate },
  { "CookieManager", RegisterCookieManager },
  { "InterceptedRequestDataImpl", RegisterInterceptedRequestData },
  { "InputStream", RegisterInputStream },
  { "JsResultHandler", RegisterJsResultHandler },
};

bool RegisterJni(JNIEnv* env) {
  return RegisterNativeMethods(env,
      kWebViewRegisteredMethods, arraysize(kWebViewRegisteredMethods));
}

} // namespace android_webview
