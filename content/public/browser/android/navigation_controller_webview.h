// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NAVIGATION_CONTROLLER_WEBVIEW_H_
#define CONTENT_PUBLIC_BROWSER_NAVIGATION_CONTROLLER_WEBVIEW_H_

class GURL;

namespace content {

struct Referrer;

// An addition to NavigationController with methods required to implement
// Android WebView.
//
// For the details, please consult the Android developer reference of
// the WebView API:
// http://developer.android.com/reference/android/webkit/WebView.html
class NavigationControllerWebView {
 public:
  // Loads a 'data:' scheme URL with specified base URL and a history entry URL.
  // This is only safe to be used for browser-initiated data: URL navigations,
  // since it shows arbitrary content as if it comes from |history_url|.
  virtual void LoadDataWithBaseURL(const GURL& data_url,
                                   const Referrer& referrer,
                                   const GURL& base_url,
                                   const GURL& history_url,
                                   bool is_overriding_user_agent) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NAVIGATION_CONTROLLER_WEBVIEW_H_
