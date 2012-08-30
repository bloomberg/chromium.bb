// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_ANDROID_PROTOCOL_ADAPTER_H_
#define CHROME_BROWSER_ANDROID_ANDROID_PROTOCOL_ADAPTER_H_

#include <jni.h>

#include "net/url_request/url_request.h"

namespace net {

class URLRequestContextGetter;

}

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
class AndroidProtocolAdapter {
 public:
  static net::URLRequest::ProtocolFactory Factory;

  // Register handlers for all supported Android protocol schemes.
  static void RegisterProtocols(
      JNIEnv* env, net::URLRequestContextGetter* context_getter);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(AndroidProtocolAdapter);
};

#endif  // CHROME_BROWSER_ANDROID_ANDROID_PROTOCOL_ADAPTER_H_
