// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_VIEW_WK_WEB_VIEW_UTIL_H_
#define IOS_WEB_WEB_VIEW_WK_WEB_VIEW_UTIL_H_

#import <WebKit/WebKit.h>

namespace web {
// Returns true if a SafeBrowsing warning is currently displayed within
// |web_view|.
bool IsSafeBrowsingWarningDisplayedInWebView(WKWebView* web_view);
}  // namespace web

#endif  // IOS_WEB_WEB_VIEW_WK_WEB_VIEW_UTIL_H_
