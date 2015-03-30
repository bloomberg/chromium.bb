// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_CRW_SIMPLE_WEB_VIEW_CONTROLLER_H_
#define IOS_WEB_WEB_STATE_UI_CRW_SIMPLE_WEB_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/web/web_state/ui/web_view_js_utils.h"

@protocol CRWSimpleWebViewControllerDelegate;

#pragma mark -
#pragma mark Web View Protocol
// An abstraction for interacting with a web view without having to know if it's
// a UIWebView or a WKWebView internally.
@protocol CRWSimpleWebViewController<NSObject>

// The page title, meant for display to the user. Will return nil if not
// available.
@property(nonatomic, readonly) NSString* title;

// The web view associated with this controller.
@property(nonatomic, readonly) UIView* view;

// The web view's scroll view.
@property(nonatomic, readonly) UIScrollView* scrollView;

// The delegate that recieves page lifecycle callbacks
@property(nonatomic, weak) id<CRWSimpleWebViewControllerDelegate> delegate;

// Reloads any displayed data to ensure the view is up to date.
- (void)reload;

// Loads a request in the WebView.
- (void)loadRequest:(NSURLRequest*)request;

// Loads an HTML document in the web view. |baseURL| is a URL that is used to
// resolve relative URLs within the document.
- (void)loadHTMLString:(NSString*)html baseURL:(NSURL*)baseURL;

// Loads a PDF file from the application sandbox. A file must exist at
// |filePath|.
- (void)loadPDFAtFilePath:(NSString*)filePath;

// Evaluates the supplied JavaScript in the web view. Calls |completionHandler|
// with results of the evaluation. The |completionHandler| can be nil.
- (void)evaluateJavaScript:(NSString*)script
       stringResultHandler:(web::JavaScriptCompletion)handler;

@end

#pragma mark -
#pragma mark Delegate Protocol
// CRWSimpleWebViewController delegate protocol for subscribing to page
// lifecycle events.
@protocol CRWSimpleWebViewControllerDelegate<NSObject>

@optional
// Called to decide if the CRWSimpleWebViewController should load a request.
- (BOOL)simpleWebViewController:
            (id<CRWSimpleWebViewController>)simpleWebViewController
     shouldStartLoadWithRequest:(NSURLRequest*)request;

// Called when the title changes. Can be called with nil value for |title|.
// Note: This call is not accurate all the time for the UIWebView case. The
// delegate method is not guaranteed to be called even if the title has changed.
// Clients should structure their code around it.
- (void)simpleWebViewController:
            (id<CRWSimpleWebViewController>)simpleWebViewController
            titleMayHaveChanged:(NSString*)title;

@end

#endif  // IOS_WEB_WEB_STATE_UI_CRW_SIMPLE_WEB_VIEW_CONTROLLER_H_
