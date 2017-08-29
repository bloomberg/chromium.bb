// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_CRW_PLACEHOLDER_NAVIGATION_INFO_H_
#define IOS_WEB_NAVIGATION_CRW_PLACEHOLDER_NAVIGATION_INFO_H_

#import <WebKit/WebKit.h>

#import "base/ios/block_types.h"

NS_ASSUME_NONNULL_BEGIN

// An NSObject wrapper for a completion handler for a placeholder navigation.
//
// A placeholder navigation is an "about:blank" page loaded into the WKWebView
// that corresponds to Native View or WebUI URL. This navigation is inserted to
// generate a WKBackForwardListItem for the Native View or WebUI URL in the
// WebView so that the WKBackForwardList contains the full list of user-visible
// navigations.
//
// Because loading in WKWebView is asynchronous, this object is attached to the
// WKNavigation for the placeholder hold, and the completion handler is expected
// to be executed in WKNavigationDelegate's |didFinishNavigation| callback to
// trigger the NativeView or WebUI HTML load.
@interface CRWPlaceholderNavigationInfo : NSObject

// Create a new instance that encapsulates a completion handler to be executed
// when |navigation| is finished.
+ (instancetype)createForNavigation:(WKNavigation*)navigation
              withCompletionHandler:(ProceduralBlock)completionHandler;

// Returns the CRWPlaceholderNavigationInfo associated with |navigation|.
+ (nullable CRWPlaceholderNavigationInfo*)infoForNavigation:
    (nullable WKNavigation*)navigation;

// Runs the completion handler saved in this object.
- (void)runCompletionHandler;

- (instancetype)init NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_NAVIGATION_CRW_PLACEHOLDER_NAVIGATION_INFO_H_
