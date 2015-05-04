// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_WEB_VIEW_CREATION_UTILS_H_
#define IOS_WEB_WEB_STATE_WEB_VIEW_CREATION_UTILS_H_

#import <UIKit/UIKit.h>

#include "ios/web/public/web_view_type.h"

@protocol CRWSimpleWebViewController;
@class WKWebView;
@class WKWebViewConfiguration;

// This file is a collection of functions that vend web views.
namespace web {
class BrowserState;

// Returns a new UIWebView for displaying regular web content and registers a
// user agent for it. The returned UIWebView will have a user agent string that
// includes the |request_group_id|.
// Note: Callers are responsible for releasing the returned UIWebView.
// TODO(shreyasv): Rename to CreateUIWebView.
UIWebView* CreateWebView(CGRect frame,
                         NSString* request_group_id,
                         BOOL use_desktop_user_agent);

// Returns a new UIWebView for displaying regular web content.
// Note: Callers are responsible for releasing the returned UIWebView.
// TODO(shreyasv): Rename to CreateUIWebView.
UIWebView* CreateWebView(CGRect frame);

// Creates and returns a WKWebView. The returned WKWebView will have a
// user agent string that includes the |request_group_id|.
// Note: Callers are responsible for releasing the returned WKWebView.
WKWebView* CreateWKWebView(CGRect frame,
                           WKWebViewConfiguration* configuration,
                           NSString* request_group_id,
                           BOOL use_desktop_user_agent);

// Returns a new WKWebView for displaying regular web content.
// WKWebViewConfiguration object for resulting web view will be obtained from
// the given |browser_state| which must not be null.
// Note: Callers are responsible for releasing the returned WKWebView.
WKWebView* CreateWKWebView(CGRect frame, BrowserState* browser_state);

// Returns the total number of WKWebViews that are currently present.
// NOTE: This only works in Debug builds and should not be used in Release
// builds.
NSUInteger GetActiveWKWebViewsCount();

// Returns a CRWSimpleWebViewController for managing/showing a web view.
// Note: Callers are responsible for releasing the CRWSimpleWebViewController.
id<CRWSimpleWebViewController> CreateSimpleWebViewController(
    CGRect frame,
    BrowserState* browser_state,
    WebViewType web_view_type);

// Returns a new CRWSimpleWebViewController subclass displaying static HTML file
// content stored in the application bundle.
// Note: Callers are responsible for releasing the returned ViewController.
id<CRWSimpleWebViewController> CreateStaticFileSimpleWebViewController(
    CGRect frame,
    BrowserState* browser_state,
    WebViewType web_view_type);

// Returns a new UIWebView subclass for displaying static HTML file content
// stored in the application bundle. if |browser_state| is nullptr, requests
// from the returned UIWebView will be done with global request context. When
// requests are made with global request context, requests such as file://
// will fail.
// Note: Callers are responsible for releasing the returned UIWebView.
UIWebView* CreateStaticFileWebView(CGRect frame, BrowserState* browser_state);

// A convenience method that returns a static file web view with
// CGRectZero and |nullptr| browser state.
// Note: Callers are responsible for releasing the returned UIWebView.
UIWebView* CreateStaticFileWebView();

}  // namespace web

#endif // IOS_WEB_WEB_STATE_WEB_VIEW_CREATION_UTILS_H_
