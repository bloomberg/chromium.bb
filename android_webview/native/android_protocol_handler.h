// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_ANDROID_PROTOCOL_HANDLER_H_
#define ANDROID_WEBVIEW_NATIVE_ANDROID_PROTOCOL_HANDLER_H_

#include "base/android/jni_android.h"
#include "base/memory/scoped_ptr.h"
#include "net/url_request/url_request_job_factory.h"

namespace net {
class URLRequestContext;
}  // namespace net

namespace android_webview {

// These method register support for Android WebView-specific protocol schemes:
//
//  - "content:" scheme is used for accessing data from Android content
//    providers, see http://developer.android.com/guide/topics/providers/
//      content-provider-basics.html#ContentURIs
//
scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
    CreateContentSchemeProtocolHandler();

//  - "file:" scheme extension for accessing application assets and resources
//    (file:///android_asset/ and file:///android_res/), see
//    http://developer.android.com/reference/android/webkit/
//      WebSettings.html#setAllowFileAccess(boolean)
scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
    CreateAssetFileProtocolHandler();

bool RegisterAndroidProtocolHandler(JNIEnv* env);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NATIVE_ANDROID_PROTOCOL_HANDLER_H_
