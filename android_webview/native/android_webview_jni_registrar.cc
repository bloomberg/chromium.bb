// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/android_webview_jni_registrar.h"

#include "android_webview/native/android_protocol_handler.h"
#include "android_webview/native/android_stream_reader_url_request_job.h"
#include "android_webview/native/android_web_view_util.h"
#include "android_webview/native/aw_contents.h"
#include "android_webview/native/aw_contents_io_thread_client.h"
#include "android_webview/native/aw_http_auth_handler.h"
#include "android_webview/native/cookie_manager.h"
#include "android_webview/native/js_result_handler.h"
#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "chrome/browser/component/web_contents_delegate_android/component_jni_registrar.h"

namespace android_webview {

static base::android::RegistrationMethod kWebViewRegisteredMethods[] = {
  { "AndroidProtocolHandler", RegisterAndroidProtocolHandler },
  { "AndroidStreamReaderUrlRequestJob",
      RegisterAndroidStreamReaderUrlRequestJob },
  { "AndroidWebViewUtil", RegisterAndroidWebViewUtil },
  { "AwContents", RegisterAwContents },
  { "AwContentsIoThreadClient", RegisterAwContentsIoThreadClient},
  { "AwHttpAuthHandler", RegisterAwHttpAuthHandler },
  { "CookieManager", RegisterCookieManager },
  { "JsResultHandler", RegisterJsResultHandler },
};

bool RegisterJni(JNIEnv* env) {
  if (!web_contents_delegate_android::RegisterJni(env))
    return false;

  return RegisterNativeMethods(env,
      kWebViewRegisteredMethods, arraysize(kWebViewRegisteredMethods));
}

} // namespace android_webview
