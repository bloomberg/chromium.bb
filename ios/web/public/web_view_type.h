// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_VIEW_TYPE_H_
#define IOS_WEB_PUBLIC_WEB_VIEW_TYPE_H_

namespace web {

// Type of web view. Different web views may provide different capabilities.
enum WebViewType {
  // Web View is UIWebView.
  UI_WEB_VIEW_TYPE = 1,
  // Web View is WKWebView.
  WK_WEB_VIEW_TYPE = 2,
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_WEB_VIEW_TYPE_H_
