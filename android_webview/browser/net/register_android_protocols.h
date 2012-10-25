// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_NET_ANDROID_PROTOCOL_HANDLER_H_
#define ANDROID_WEBVIEW_BROWSER_NET_ANDROID_PROTOCOL_HANDLER_H_

namespace net {
class URLRequestJobFactory;
}  // namespace net

namespace android_webview {

// This class adds support for Android WebView-specific protocol schemes:
//
//  - "content:" scheme is used for accessing data from Android content
//    providers, see http://developer.android.com/guide/topics/providers/
//      content-provider-basics.html#ContentURIs
//
//  - "file:" scheme extension for accessing application assets and resources
//    (file:///android_asset/ and file:///android_res/), see
//    http://developer.android.com/reference/android/webkit/
//      WebSettings.html#setAllowFileAccess(boolean)
//
void RegisterAndroidProtocolsOnIOThread(
    net::URLRequestJobFactory* job_factory);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_NET_ANDROID_PROTOCOL_HANDLER_H_
