// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_VIEW_UTIL_H_
#define IOS_WEB_WEB_VIEW_UTIL_H_

namespace web {

// Returns true if WKWebView is being used instead of UIWebView.
// TODO(stuartmorgan): Eliminate this global flag in favor of a per-web-view
// flag.
bool IsWKWebViewEnabled();

// If |flag| is true, causes IsWKWebViewEnabled to return false, even if
// WKWebView is enabled using the compile time flag. Should only be called from
// ScopedUIWebViewEnforcer for use in unit tests that need to test UIWebView
// while WKWebView is enabled.
void SetForceUIWebView(bool flag);

// Returns true if use of UIWebView is to be enforced.
bool GetForceUIWebView();

}  // web

#endif  // IOS_WEB_WEB_VIEW_UTIL_H_
