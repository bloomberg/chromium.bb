// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_VIEW_CREATION_UTIL_H_
#define IOS_WEB_PUBLIC_WEB_VIEW_CREATION_UTIL_H_

#import <CoreGraphics/CoreGraphics.h>

@class WKWebView;

namespace web {
class BrowserState;

// Returns true if WKWebView is supported on current OS/platform/arch.
bool IsWKWebViewSupported();

// Returns a new WKWebView for displaying regular web content.
// WKWebViewConfiguration object for resulting web view will be obtained from
// the given |browser_state|.
//
// Preconditions for creation of a WKWebView:
// 1) |browser_state| is not null.
// 2) web::BrowsingDataPartition is synchronized.
//
// Note: Callers are responsible for releasing the returned WKWebView.
WKWebView* CreateWKWebView(CGRect frame, BrowserState* browser_state);

}  // web

#endif  // IOS_WEB_PUBLIC_WEB_VIEW_CREATION_UTIL_H_
