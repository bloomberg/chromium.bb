// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_WEB_VIEW_INTERNAL_CREATION_UTIL_H_
#define IOS_WEB_WEB_STATE_WEB_VIEW_INTERNAL_CREATION_UTIL_H_

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
//
// Preconditions for creation of a WKWebView:
// 1) |browser_state|, |configuration| are not null.
// 2) web::BrowsingDataPartition is synchronized.
// 3) The WKProcessPool of the configuration is the same as the WKProcessPool
//    of the WKWebViewConfiguration associated with |browser_state|.
//
// Note: Callers are responsible for releasing the returned WKWebView.
WKWebView* CreateWKWebView(CGRect frame,
                           WKWebViewConfiguration* configuration,
                           BrowserState* browser_state,
                           NSString* request_group_id,
                           BOOL use_desktop_user_agent);

// Creates and returns a new WKWebView for displaying regular web content.
// The preconditions for the creation of a WKWebView are the same as the
// previous method.
// Note: Callers are responsible for releasing the returned WKWebView.
WKWebView* CreateWKWebView(CGRect frame,
                           WKWebViewConfiguration* configuration,
                           BrowserState* browser_state);

// Returns the total number of WKWebViews that are currently present.
// NOTE: This only works in Debug builds and should not be used in Release
// builds.
// DEPRECATED. Please use web::WebViewCounter instead.
// TODO(shreyasv): Remove this once all callers have stopped using it.
// crbug.com/480507
NSUInteger GetActiveWKWebViewsCount();

// Returns a CRWSimpleWebViewController for managing/showing a web view.
// The BrowsingDataPartition must be synchronized before this method is called.
// Note: Callers are responsible for releasing the CRWSimpleWebViewController.
id<CRWSimpleWebViewController> CreateSimpleWebViewController(
    CGRect frame,
    BrowserState* browser_state,
    WebViewType web_view_type);

// Returns a new CRWSimpleWebViewController subclass displaying static HTML file
// content stored in the application bundle.
// The BrowsingDataPartition must be synchronized before this method is called.
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

#if !defined(NDEBUG)
// Returns true if the creation of web views using alloc, init has been allowed
// by the embedder.
bool IsWebViewAllocInitAllowed();
#endif

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_WEB_VIEW_INTERNAL_CREATION_UTIL_H_
