// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_NAVIGATION_DELEGATE_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_NAVIGATION_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

@protocol CRIWVTranslateDelegate;
@class CWVWebView;

// Navigation delegate protocol for CWVWebViews.  Allows embedders to hook
// page loading and receive events for navigation.
CWV_EXPORT
@protocol CWVNavigationDelegate<NSObject>
@optional

// Asks delegate if WebView should start the load. WebView will
// load the request if this method is not implemented.
- (BOOL)webView:(CWVWebView*)webView
    shouldStartLoadWithRequest:(NSURLRequest*)request;

// Asks delegate if WebView should continue the load. WebView
// will load the response if this method is not implemented.
- (BOOL)webView:(CWVWebView*)webView
    shouldContinueLoadWithResponse:(NSURLResponse*)response;

// Notifies the delegate that main frame navigation has started.
- (void)webViewDidStartProvisionalNavigation:(CWVWebView*)webView;

// Notifies the delegate that response data started arriving for
// the main frame.
- (void)webViewDidCommitNavigation:(CWVWebView*)webView;

// Notifies the delegate that page load is completed. Called for
// both success and failure cases.
- (void)webView:(CWVWebView*)webView didLoadPageWithSuccess:(BOOL)success;

// Notifies the delegate that web view process was terminated
// (usually by crashing, though possibly by other means).
- (void)webViewWebContentProcessDidTerminate:(CWVWebView*)webView;

@end

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_NAVIGATION_DELEGATE_H_
