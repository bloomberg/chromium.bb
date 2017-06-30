// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_WEB_VIEW_NAVIGATION_PROXY_H_
#define IOS_WEB_WEB_STATE_UI_WEB_VIEW_NAVIGATION_PROXY_H_

#import <WebKit/WebKit.h>

NS_ASSUME_NONNULL_BEGIN

// A protocol to expose a subset of the WKWebView API to NavigationManager.
@protocol CRWWebViewNavigationProxy

@property(nonatomic, readonly, strong) WKBackForwardList* backForwardList;
@property(nonatomic, readonly) BOOL canGoBack;
@property(nonatomic, readonly) BOOL canGoForward;

- (nullable WKNavigation*)goBack;
- (nullable WKNavigation*)goForward;
- (nullable WKNavigation*)goToBackForwardListItem:(WKBackForwardListItem*)item;

@end
NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_WEB_STATE_UI_WEB_VIEW_NAVIGATION_PROXY_H_
