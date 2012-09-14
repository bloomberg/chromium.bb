// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_HTTP_AUTH_HANDLER_BASE_H_
#define ANDROID_WEBVIEW_BROWSER_AW_HTTP_AUTH_HANDLER_BASE_H_

namespace content {
class WebContents;
};

namespace net {
class AuthChallengeInfo;
};

namespace android_webview {

class AwLoginDelegate;

// browser/ layer interface for AwHttpAuthHandler (which is implemented in the
// native/ layer as a native version of the Java class of the same name). This
// allows the browser/ layer to be unaware of JNI/Java shenanigans.
class AwHttpAuthHandlerBase {
 public:
  static AwHttpAuthHandlerBase* Create(AwLoginDelegate* login_delegate,
                                       net::AuthChallengeInfo* auth_info);
  virtual ~AwHttpAuthHandlerBase();

  // Provides an 'escape-hatch' out to Java for the browser/ layer
  // AwLoginDelegate.
  virtual void HandleOnUIThread(content::WebContents*) = 0;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_HTTP_AUTH_HANDLER_BASE_H_
